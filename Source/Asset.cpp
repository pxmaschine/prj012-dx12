#include <Asset.h>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb/stb_image.h>

#include <Platform/Platform.h>
#include <Platform/Jobs.h>

#include <Platform/PlatformContext.h>
#if ZV_COMPILER_CL
#pragma warning(disable: 4996)  // This function or variable may be unsafe
#endif

// #define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include <ThirdParty/cgltf/cgltf.h>

#include <AssetTable.cpp>

namespace { class AssetManager; }
static UniquePtr<AssetManager> s_asset_manager{ nullptr };

namespace
{
    SubmeshHandle cgltf_append_submesh(
        ModelAsset* asset,
        const MeshGeometryData& geom,
        const MaterialInfo& material_info,
        const Matrix& local,
        const Matrix& world,
        SubmeshHandle parent)
    {
        SubmeshData data{};
        data.m_data.m_vertices.resize(geom.m_vertices.size());
        data.m_data.m_indices.resize(geom.m_indices.size());
        if (geom.m_vertices.size())
        {
            memcpy(data.m_data.m_vertices.data(), geom.m_vertices.data(), sizeof(MeshVertex) * geom.m_vertices.size());
        }
        if (geom.m_indices.size())
        {
            memcpy(data.m_data.m_indices.data(), geom.m_indices.data(), sizeof(u16) * geom.m_indices.size());
        }

        data.m_local_transform = local;
        data.m_world_transform = world;
        data.m_material_info = material_info;
        data.m_parent = parent;

        const SubmeshHandle handle = (SubmeshHandle)(s32)asset->m_submeshes.size();
        asset->m_submeshes.push_back(data);

        if ((s32)parent != (s32)SubmeshHandle::Invalid)
        {
            asset->m_submeshes[(s32)parent].m_children.push_back(handle);
        }

        return handle;
    }
    
    void cgltf_read_material_info(const cgltf_primitive* prim, const char* model_id, MaterialInfo* out_material_info)
    {
        if (prim->material)
        {
            const cgltf_material* material = prim->material;

            if (material->has_pbr_metallic_roughness)
            {
                cgltf_texture* albedo_texture = material->pbr_metallic_roughness.base_color_texture.texture;
                if (albedo_texture && albedo_texture->image)
                {
                    FixedSizeString<128> albedo_texture_name{};
                    albedo_texture_name.assign(model_id);
                    albedo_texture_name.append("/");
                    albedo_texture_name.append(albedo_texture->image->uri);
                    out_material_info->m_albedo_texture_id.set(albedo_texture_name.c_str());

                    if (albedo_texture->sampler)
                    {
                        cgltf_wrap_mode wrap_s = albedo_texture->sampler->wrap_s;
                        cgltf_wrap_mode wrap_t = albedo_texture->sampler->wrap_t;

                        if (wrap_s != wrap_t)
                        {
                            zv_warning("Different wrap modes for S and T currently not supported");
                        }
                        else
                        {
                            switch (wrap_s)
                            {
                                case cgltf_wrap_mode_repeat:
                                    out_material_info->m_sampler_mode = SamplerAddressMode::Wrap;
                                    break;
                                case cgltf_wrap_mode_mirrored_repeat:
                                    out_material_info->m_sampler_mode = SamplerAddressMode::Wrap;
                                    break;
                                case cgltf_wrap_mode_clamp_to_edge:
                                    out_material_info->m_sampler_mode = SamplerAddressMode::Clamp;
                                    break;
                            }
                        }
                    }
                }
                cgltf_texture* normal_texture = material->normal_texture.texture;
                if (normal_texture && normal_texture->image)
                {
                    FixedSizeString<128> normal_texture_name{};
                    normal_texture_name.assign(model_id);
                    normal_texture_name.append("/");
                    normal_texture_name.append(normal_texture->image->uri);
                    out_material_info->m_normal_texture_id.set(normal_texture_name.c_str());
                }
                cgltf_texture* metallic_roughness_texture = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                if (metallic_roughness_texture && metallic_roughness_texture->image)
                {
                    FixedSizeString<128> metallic_roughness_texture_name{};
                    metallic_roughness_texture_name.assign(model_id);
                    metallic_roughness_texture_name.append("/");
                    metallic_roughness_texture_name.append(metallic_roughness_texture->image->uri);
                    out_material_info->m_metallic_roughness_texture_id.set(metallic_roughness_texture_name.c_str());

                    auto asset_id = out_material_info->m_metallic_roughness_texture_id;

                    // TODO: This is not true all the time!!!
                    out_material_info->m_channel_packing = ChannelPacking::Metalness | ChannelPacking::Roughness;
                }
                cgltf_texture* ao_texture = material->occlusion_texture.texture;
                if (ao_texture && ao_texture->image)
                {
                    FixedSizeString<128> ao_texture_name{};
                    ao_texture_name.assign(model_id);
                    ao_texture_name.append("/");
                    ao_texture_name.append(ao_texture->image->uri);
                    out_material_info->m_ao_texture_id.set(ao_texture_name.c_str());
                }
                cgltf_texture* emissive_texture = material->emissive_texture.texture;
                if (emissive_texture && emissive_texture->image)
                {
                    FixedSizeString<128> emissive_texture_name{};
                    emissive_texture_name.assign(model_id);
                    emissive_texture_name.append("/");
                    emissive_texture_name.append(emissive_texture->image->uri);
                    out_material_info->m_emissive_texture_id.set(emissive_texture_name.c_str());
                }

                out_material_info->m_albedo_color = Vector3(
                    material->pbr_metallic_roughness.base_color_factor[0], 
                    material->pbr_metallic_roughness.base_color_factor[1], 
                    material->pbr_metallic_roughness.base_color_factor[2]);
                out_material_info->m_roughness = material->pbr_metallic_roughness.roughness_factor;
                out_material_info->m_metalness = material->pbr_metallic_roughness.metallic_factor;
                out_material_info->m_specular = material->specular.specular_factor;
                // out_material_info->m_specular = material->ior.ior;
                out_material_info->m_emissive = material->emissive_strength.emissive_strength;
            }
        }
    }

    inline const cgltf_accessor* cgltf_find_attr_accessor(const cgltf_primitive* prim, cgltf_attribute_type type, s32 index)
    {
        for (cgltf_size i = 0; i < prim->attributes_count; ++i)
        {
            const cgltf_attribute* a = &prim->attributes[i];
            if (a->type == type && a->index == (cgltf_int)index)
            {
                return a->data;
            }
        }
        return nullptr;
    }

    inline void cgltf_read_indices(const cgltf_accessor* acc, DynamicArray<u16>& out_indices, bool is_cw_winding_order = true)
    {
        cgltf_size num_indices = cgltf_accessor_unpack_indices(acc, nullptr, 0, 0);
        out_indices.resize((size_t)num_indices);
        cgltf_accessor_unpack_indices(acc, out_indices.data(), sizeof(u16), num_indices);

        if (is_cw_winding_order)
        {
            for (s32 i = 0; i + 2 < (s32)out_indices.size(); i += 3)
            {
                const u16 t = out_indices[i + 1];
                out_indices[i + 1]   = out_indices[i + 2];
                out_indices[i + 2]   = t;
            }
        }
    }

    void cgltf_read_vertices(const cgltf_primitive* prim, DynamicArray<MeshVertex>& out_vertices, bool is_left_handed_coordinate_system = true)
    {
        const cgltf_accessor* pos_acc  = cgltf_find_attr_accessor(prim, cgltf_attribute_type_position, 0);
        // Position is mandatory for us; if missing, produce zero verts.
        if (!pos_acc)
        {
            out_vertices.resize(0);
            return;
        }
    
        const cgltf_size vcount = pos_acc->count;
        out_vertices.resize((size_t)vcount);
        MeshVertex* vtx = out_vertices.data();
    
        const cgltf_accessor* uv_acc   = cgltf_find_attr_accessor(prim, cgltf_attribute_type_texcoord, 0);
        const cgltf_accessor* nrm_acc  = cgltf_find_attr_accessor(prim, cgltf_attribute_type_normal,   0);
        const cgltf_accessor* tan_acc  = cgltf_find_attr_accessor(prim, cgltf_attribute_type_tangent,  0);
    
        f32 tmp[4];
    
        for (cgltf_size i = 0; i < vcount; ++i)
        {
            // position
            tmp[0]=tmp[1]=tmp[2]=0.0f; tmp[3]=1.0f;
            cgltf_accessor_read_float(pos_acc, i, tmp, 3);
            vtx[i].position.x = tmp[0];
            vtx[i].position.y = tmp[1];
            vtx[i].position.z = tmp[2];
            if (is_left_handed_coordinate_system)
            {
                vtx[i].position = basis_flip_y(vtx[i].position);
            }
    
            // uv
            if (uv_acc)
            {
                tmp[0]=tmp[1]=0.0f;
                cgltf_accessor_read_float(uv_acc, i, tmp, 2);
                vtx[i].uv.x = tmp[0];
                vtx[i].uv.y = tmp[1];
            }
            else
            {
                vtx[i].uv.x = 0.0f; vtx[i].uv.y = 0.0f;
            }
    
            // normal
            if (nrm_acc)
            {
                tmp[0]=tmp[1]=tmp[2]=0.0f;
                cgltf_accessor_read_float(nrm_acc, i, tmp, 3);
                vtx[i].normal.x = tmp[0];
                vtx[i].normal.y = tmp[1];
                vtx[i].normal.z = tmp[2];
                if (is_left_handed_coordinate_system)
                {
                    vtx[i].normal = basis_flip_y(vtx[i].normal);
                }
            }
            else
            {
                vtx[i].normal.x = 0.0f; vtx[i].normal.y = 0.0f; vtx[i].normal.z = 1.0f;
            }
    
            // tangent (xyz from accessor, ignore handedness w)
            if (tan_acc)
            {
                tmp[0]=tmp[1]=tmp[2]=0.0f; tmp[3]=1.0f;
                cgltf_accessor_read_float(tan_acc, i, tmp, 4);
                vtx[i].tangent.x = tmp[0];
                vtx[i].tangent.y = tmp[1];
                vtx[i].tangent.z = tmp[2];
                vtx[i].tangent.w = tmp[3];
                if (is_left_handed_coordinate_system)
                {
                    vtx[i].tangent = basis_flip_y(vtx[i].tangent);
                }
            }
            else
            {
                vtx[i].tangent.x = 1.0f; vtx[i].tangent.y = 0.0f; vtx[i].tangent.z = 0.0f;
                vtx[i].tangent.w = 1.0f;
            }
        }
    }

    void cgltf_read_geometry_data(const cgltf_primitive* prim, MeshGeometryData* out_geom)
    {
        cgltf_read_vertices(prim, out_geom->m_vertices);
    
        if (prim->indices && prim->type == cgltf_primitive_type_triangles)
        {
            cgltf_read_indices(prim->indices, out_geom->m_indices);
        }
        else
        {
            zv_error("No index accessor found for primitive");
        }
    }

    inline Matrix cgltf_get_local_transform(const cgltf_node* node, bool is_row_major = true)
    {
        f32 cm[16]; // column-major from cgltf/glTF
        cgltf_node_transform_local(node, cm);
    
        if (!is_row_major)
        {
            // TODO: Parameterize handedeness flip
            return basis_flip_y(Matrix{ cm });
        }

        // Convert to row-major memory layout
        f32 rm[16] = {
            cm[0],  cm[4],  cm[8],  cm[12],
            cm[1],  cm[5],  cm[9],  cm[13],
            cm[2],  cm[6],  cm[10], cm[14],
            cm[3],  cm[7],  cm[11], cm[15],
        };
        // TODO: Parameterize handedeness flip
        return basis_flip_y(Matrix{ rm });
    }

    inline Matrix cgltf_get_world_transform(const cgltf_node* node, bool is_row_major = true)
    {
        f32 cm[16]; // column-major from cgltf/glTF
        cgltf_node_transform_world(node, cm);
    
        if (!is_row_major)
        {
            // TODO: Parameterize handedeness flip
            return basis_flip_y(Matrix{ cm });
        }

        // Convert to row-major memory layout
        f32 rm[16] = {
            cm[0],  cm[4],  cm[8],  cm[12],
            cm[1],  cm[5],  cm[9],  cm[13],
            cm[2],  cm[6],  cm[10], cm[14],
            cm[3],  cm[7],  cm[11], cm[15],
        };
        // TODO: Parameterize handedeness flip
        return basis_flip_y(Matrix{ rm });
    }

    void cgltf_parse_node(const ModelLoadInfo& load_info, const cgltf_node* node, ModelAsset* asset, SubmeshHandle parent_handle)
    {
        if (node->mesh)
        {
            const Matrix local = cgltf_get_local_transform(node);
            const Matrix world = cgltf_get_world_transform(node);
    
            for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p)
            {
                const cgltf_primitive* prim = &node->mesh->primitives[p];
    
                MeshGeometryData geom{};
                cgltf_read_geometry_data(prim, &geom);
                MaterialInfo material_info{};
                cgltf_read_material_info(prim, asset->m_id.name().c_str(), &material_info);
    
                cgltf_append_submesh(asset, geom, material_info, local, world, parent_handle);
            }
        }
    
        for (cgltf_size i = 0; i < node->children_count; ++i)
        {
            cgltf_parse_node(load_info, node->children[i], asset, parent_handle);
        }
    }

    void cgltf_parse_model_data(const ModelLoadInfo& load_info, cgltf_scene* scene, ModelAsset* out_asset)
    {
        zv_assert_msg(scene != nullptr, "Invalid cgltf_scene passed to cgltf_parse_model_data");
        zv_assert_msg(out_asset != nullptr, "Invalid out_asset passed to cgltf_parse_model_data");

        // Clear ModelAsset
        out_asset->m_submeshes.resize(0);

        for (cgltf_size i = 0; i < scene->nodes_count; ++i)
        {
            cgltf_parse_node(load_info, scene->nodes[i], out_asset, SubmeshHandle::Invalid);
        }
    }

    // inline AssetState get_asset_state(Asset* asset)
    // {
    //     return asset->m_state.load(std::memory_order_acquire);
    // }

    class AssetManager : public Singleton<AssetManager>
    {
    private:
        HashMap<AssetId, TextureAsset> m_texture_assets;
        HashMap<AssetId, ModelAsset> m_model_assets;

        Mutex m_tex_mutex;
        HashSet<AssetId> m_tex_inflight;

        // For async model loading
        Mutex m_model_mutex;
        HashSet<AssetId> m_model_inflight;
    
        struct TextureLoadJob
        {
            AssetManager* manager;
            AssetId id;
            bool flip_vertically;
        };

        struct ModelLoadJob
        {
            AssetManager* manager;
            AssetId id;
        };
    
        static void load_texture_asset_job(JobQueue* queue, void* data);
        static void load_model_asset_job(JobQueue* queue, void* data);

    public:
        AssetManager() : BaseType(this) {}

        // void load_texture_asset_async(const AssetId& id, bool flip_vertically);

        void load_texture_asset_async(const AssetId& id, bool flip_vertically = false);
        TextureAsset* get_texture_asset(const AssetId& id);

        void load_model_asset(const AssetId& id);
        void load_model_asset_async(const AssetId& id);
        ModelAsset* get_model_asset(const AssetId& id);

    private:
        TextureLoadInfo get_texture_load_info(const AssetId& id) const;
        ModelLoadInfo get_model_load_info(const AssetId& id) const;
    };

    void AssetManager::load_texture_asset_job(JobQueue*, void* data)
    {
        UniquePtr<TextureLoadJob> job(static_cast<TextureLoadJob*>(data)); // auto-delete
        AssetManager* manager = job->manager;
        const AssetId id   = job->id;

        // Gather info (safe; pure read)
        TextureLoadInfo load_info = manager->get_texture_load_info(id);

        stbi_set_flip_vertically_on_load_thread(job->flip_vertically ? 1 : 0);

        s32 width = 0, height = 0, original_channels = 0;
        u8* pixels = stbi_load(load_info.m_path, &width, &height,
                                        &original_channels, load_info.m_request_channels);
        if (!pixels)
        {
            zv_error("Failed to load texture file: {}", load_info.m_path);
            // Clear inflight so a future call can retry
            ScopedLock lock(manager->m_tex_mutex);
            manager->m_tex_inflight.erase(id);
            return;
        }

        const u32 total_size = static_cast<u32>(width) *
                               static_cast<u32>(height) *
                               static_cast<u32>(load_info.m_request_channels);

        if (original_channels != load_info.m_request_channels)
        {
            // TODO
            // zv_warning("Original channels ({}) != request channels ({}) for texture {}",
            //         original_channels, load_info.m_request_channels, load_info.m_path);
        }

        // Build the asset off-thread, no locks held
        TextureAsset asset{};
        // asset.m_state         = AssetState::Loaded;
        asset.m_id            = id;
        asset.m_width         = width;
        asset.m_height        = height;
        asset.m_num_channels  = load_info.m_request_channels;
        asset.m_data          = make_unique_ptr<u8[]>(total_size);
        asset.m_dimension     = TextureDimension::Texture2D;
        asset.m_format        = load_info.m_format;

        memcpy(asset.m_data.get(), pixels, total_size);
        stbi_image_free(pixels);

        // Publish under the mutex
        {
            ScopedLock lock(manager->m_tex_mutex);

            // Another thread might have synchronously loaded it meanwhile
            if (manager->m_texture_assets.find(id) == manager->m_texture_assets.end())
            {
                manager->m_texture_assets.emplace(id, move_ptr(asset));
            }

            manager->m_tex_inflight.erase(id);
        }
    }

    void AssetManager::load_texture_asset_async(const AssetId& id, bool flip_vertically)
    {
        zv_assert_msg(id.is_valid(), "Invalid asset id passed to load_texture_asset_async");

        {
            ScopedLock lock (m_tex_mutex);

            if (m_texture_assets.find(id) != m_texture_assets.end())
            {
                return;
            }
            
            if (!m_tex_inflight.insert(id).second)
            {
                return; // already queued
            }
        }

        // Allocate a tiny job record; worker deletes it when done.
        auto* job = new TextureLoadJob{ this, id, flip_vertically };
        Platform::add_job(JobPriority::High, &AssetManager::load_texture_asset_job, job);
    }

    TextureAsset* AssetManager::get_texture_asset(const AssetId& id)
    {
        if (!id.is_valid())
        {
            zv_error("Invalid asset id passed to get_texture_asset");
            return nullptr;
        }

        ScopedLock lock(m_tex_mutex);

        auto it = m_texture_assets.find(id);
        if (it == m_texture_assets.end())
        {
            // TODO
            // zv_warning("Asset not found in texture assets map");
            return nullptr;
        }

        return &it->second;
    }

    // void AssetManager::load_model_asset(const AssetId& id)
    // {
    //     zv_assert_msg(id.is_valid(), "Invalid asset id passed to load_model_asset");

    //     if (m_model_assets.find(id) != m_model_assets.end())
    //     {
    //         return;
    //     }

    //     ModelLoadInfo load_info = get_model_load_info(id);

    //     cgltf_options options{};
    //     cgltf_data* data = nullptr;
    //     if (cgltf_parse_file(&options, load_info.m_path, &data) != cgltf_result_success)
    //     {
    //         zv_error("Failed to parse model file: {}", load_info.m_path);
    //         return;
    //     }

    //     if (cgltf_load_buffers(&options, data, load_info.m_path) != cgltf_result_success)
    //     {
    //         zv_error("Failed to load buffers for model file: {}", load_info.m_path);
    //         return;
    //     }

    //     if (data->scenes_count > 1)
    //     {
    //         zv_warning("Model file '{}' has multiple scenes, only the first one will be used", load_info.m_path);
    //     }

    //     if (data->scenes_count == 0)
    //     {
    //         zv_warning("No model data found for model file '{}'", load_info.m_path);
    //         return;
    //     }

    //     ModelAsset asset{ id };
    //     cgltf_parse_model_data(load_info, &data->scenes[0], &asset);
    //     cgltf_free(data);

    //     // asset.m_state = AssetState::Loaded;

    //     m_model_assets.emplace(id, move_ptr(asset));
    // }

    // --- ASYNC MODEL LOADING LOGIC STARTS HERE ---

    void AssetManager::load_model_asset_job(JobQueue*, void* data)
    {
        UniquePtr<ModelLoadJob> job(static_cast<ModelLoadJob*>(data)); // auto-delete
        AssetManager* manager = job->manager;
        const AssetId id = job->id;

        // Gather info (safe; pure read)
        ModelLoadInfo load_info = manager->get_model_load_info(id);

        cgltf_options options{};
        cgltf_data* cgltfData = nullptr;
        if (cgltf_parse_file(&options, load_info.m_path, &cgltfData) != cgltf_result_success)
        {
            zv_error("Failed to parse model file: {}", load_info.m_path);
            // Clear inflight so a future call can retry
            ScopedLock lock(manager->m_model_mutex);
            manager->m_model_inflight.erase(id);
            return;
        }

        if (cgltf_load_buffers(&options, cgltfData, load_info.m_path) != cgltf_result_success)
        {
            zv_error("Failed to load buffers for model file: {}", load_info.m_path);
            ScopedLock lock(manager->m_model_mutex);
            manager->m_model_inflight.erase(id);
            cgltf_free(cgltfData);
            return;
        }

        if (cgltfData->scenes_count > 1)
        {
            zv_warning("Model file '{}' has multiple scenes, only the first one will be used", load_info.m_path);
        }

        if (cgltfData->scenes_count == 0)
        {
            zv_warning("No model data found for model file '{}'", load_info.m_path);
            ScopedLock lock(manager->m_model_mutex);
            manager->m_model_inflight.erase(id);
            cgltf_free(cgltfData);
            return;
        }

        // Build the asset off-thread, no locks held
        ModelAsset asset{ id };
        cgltf_parse_model_data(load_info, &cgltfData->scenes[0], &asset);
        cgltf_free(cgltfData);

        // Publish under the mutex
        {
            ScopedLock lock(manager->m_model_mutex);

            // Another thread might have synchronously loaded it meanwhile
            if (manager->m_model_assets.find(id) == manager->m_model_assets.end())
            {
                manager->m_model_assets.emplace(id, move_ptr(asset));
            }

            manager->m_model_inflight.erase(id);
        }
    }

    void AssetManager::load_model_asset_async(const AssetId& id)
    {
        zv_assert_msg(id.is_valid(), "Invalid asset id passed to load_model_asset_async");

        {
            ScopedLock lock(m_model_mutex);

            if (m_model_assets.find(id) != m_model_assets.end())
            {
                return;
            }

            if (!m_model_inflight.insert(id).second)
            {
                return; // already queued
            }
        }

        // Allocate a tiny job record; worker deletes it when done.
        auto* job = new ModelLoadJob{ this, id };
        Platform::add_job(JobPriority::High, &AssetManager::load_model_asset_job, job);
    }

    // --- END ASYNC MODEL LOADING LOGIC ---

    ModelAsset* AssetManager::get_model_asset(const AssetId& id)
    {
        if (!id.is_valid())
        {
            zv_warning("Invalid asset id passed to get_model_asset");
            return nullptr;
        }

        auto it = m_model_assets.find(id);
        if (it == m_model_assets.end())
        {
            zv_warning("Asset not found in model assets map");
            return nullptr;
        }
        return &it->second;
    }

    TextureLoadInfo AssetManager::get_texture_load_info(const AssetId& id) const
    {
        zv_assert_msg(s_texture_load_infos.find(id) != s_texture_load_infos.end(), "Texture load info not found for asset id: {}", id.name().c_str());
        return s_texture_load_infos[id];
    }
    
    ModelLoadInfo AssetManager::get_model_load_info(const AssetId& id) const
    {
        zv_assert_msg(s_model_load_infos.find(id) != s_model_load_infos.end(), "Model load info not found for asset id: {}", id.name().c_str());
        return s_model_load_infos[id];
    }
}

void Assets::initialize()
{
    zv_assert_msg(s_asset_manager == nullptr, "Asset manager already initialized!");
    s_asset_manager = make_unique_ptr<AssetManager>();
}

void Assets::shutdown()
{
    s_asset_manager = nullptr;
}

void Assets::load_texture_asset_async(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");
    s_asset_manager->load_texture_asset_async(id);
}

void Assets::load_texture_asset(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");

    s_asset_manager->load_texture_asset_async(id);

    Platform::complete_all_jobs(JobPriority::High);
}

TextureAsset* Assets::get_texture_asset(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");
    return s_asset_manager->get_texture_asset(id);
}

void Assets::load_model_asset_async(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");
    s_asset_manager->load_model_asset_async(id);
}

void Assets::load_model_asset(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");

    s_asset_manager->load_model_asset_async(id);

    Platform::complete_all_jobs(JobPriority::High);
}

ModelAsset* Assets::get_model_asset(const AssetId& id)
{
    zv_assert_msg(s_asset_manager != nullptr, "Asset manager not initialized!");
    return s_asset_manager->get_model_asset(id);
}
