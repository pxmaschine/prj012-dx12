#include <Geometry.h>

namespace
{
    // Taken from: https://iquilezles.org/articles/normals/
    void recalculate_normals(MeshVertex* vertices, size_t num_vertices, u16* indices, size_t num_indices)
    {
        for (u32 i = 0; i < num_vertices; ++i) vertices[i].normal = Vector3(0.0f);
    
        for (u32 i = 0; i < num_indices; i += 3)
        {
            const u16 ia = indices[i + 0];
            const u16 ib = indices[i + 1];
            const u16 ic = indices[i + 2];
    
            const Vector3 e1 = vertices[ib].position - vertices[ia].position;
            const Vector3 e2 = vertices[ic].position - vertices[ia].position;
            const Vector3 no = e1.Cross(e2);
    
            vertices[ia].normal += no;
            vertices[ib].normal += no;
            vertices[ic].normal += no;
        }
    
        for (u32 i = 0; i < num_vertices; ++i) vertices[i].normal.Normalize();
    }

    MeshVertex get_mid_point(const MeshVertex& v0, const MeshVertex& v1)
    {
        Vector3 p0 = v0.position;
        Vector3 p1 = v1.position;

        Vector3 n0 = v0.normal;
        Vector3 n1 = v1.normal;

        Vector3 tan0 = v0.tangent;
        Vector3 tan1 = v1.tangent;

        Vector2 tex0 = v0.uv;
        Vector2 tex1 = v1.uv;

        // Compute the midpoints of all the attributes.  Vectors need to be normalized
        // since linear interpolating can make them not unit length.  
        Vector3 pos = 0.5f * (p0 + p1);
        Vector3 normal = Vector3(0.5f * (n0 + n1));
        normal.Normalize();
        Vector3 tangent = Vector3(0.5f * (tan0 + tan1));
        tangent.Normalize();
        Vector2 tex = 0.5f * (tex0 + tex1);

        MeshVertex v;
        v.position = pos;
        v.normal = normal;
        v.tangent = Vector4(tangent.x, tangent.y, tangent.z, 1.0f);
        v.uv = tex;

        return v;
    }

    void subdivide(PrimitiveMeshGeometryData& data)
    {
        // Save a copy of the input geometry.
        PrimitiveMeshGeometryData input_copy = data;

        data.m_vertices.resize(0);
        data.m_indices.resize(0);

        //       v1
        //       *
        //      / \
        //     /   \
        //  m0*-----*m1
        //   / \   / \
        //  /   \ /   \
        // *-----*-----*
        // v0    m2     v2

        u32 num_tris = static_cast<u32>(input_copy.m_indices.size() / 3);
        for(u32 i = 0; i < num_tris; ++i)
        {
            MeshVertex v0 = input_copy.m_vertices[input_copy.m_indices[i * 3 + 0]];
            MeshVertex v1 = input_copy.m_vertices[input_copy.m_indices[i * 3 + 1]];
            MeshVertex v2 = input_copy.m_vertices[input_copy.m_indices[i * 3 + 2]];

            //
            // Generate the midpoints.
            //

            MeshVertex m0 = get_mid_point(v0, v1);
            MeshVertex m1 = get_mid_point(v1, v2);
            MeshVertex m2 = get_mid_point(v0, v2);

            //
            // Add new geometry.
            //

            data.m_vertices.push_back(v0); // 0
            data.m_vertices.push_back(v1); // 1
            data.m_vertices.push_back(v2); // 2
            data.m_vertices.push_back(m0); // 3
            data.m_vertices.push_back(m1); // 4
            data.m_vertices.push_back(m2); // 5
    
            data.m_indices.push_back(static_cast<u16>(i * 6 + 0));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 3));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 5));

            data.m_indices.push_back(static_cast<u16>(i * 6 + 3));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 4));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 5));

            data.m_indices.push_back(static_cast<u16>(i * 6 + 5));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 4));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 2));

            data.m_indices.push_back(static_cast<u16>(i * 6 + 3));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 1));
            data.m_indices.push_back(static_cast<u16>(i * 6 + 4));
        }
    }

    void build_cylinder_side(
        f32 bottom_radius, f32 top_radius, f32 height,
        u32 radial_segments, u32 height_segments,
        PrimitiveMeshGeometryData& data)
    {
        //
        // Build Stacks.
        // 

        f32 stack_height = height / height_segments;

        // Amount to increment radius as we move up each stack level from bottom to top.
        f32 radius_step = (top_radius - bottom_radius) / height_segments;

        u32 ring_count = height_segments + 1;

        // Compute vertices for each stack ring starting at the bottom and moving up.
        for (u32 i = 0; i < ring_count; ++i)
        {
            f32 y = -0.5f * height + i * stack_height;
            f32 r = bottom_radius + i * radius_step;

            // vertices of ring
            f32 d_theta = 2.0f * ZV_PI / radial_segments;
            for (u32 j = 0; j <= radial_segments; ++j)
            {
                MeshVertex vertex;

                f32 c = cosf(j * d_theta);
                f32 s = sinf(j * d_theta);

                vertex.position.x = r * c;
                vertex.position.y = y;
                vertex.position.z = r * s;

                vertex.uv.x = (f32)j / radial_segments;
                vertex.uv.y = 1.0f - (f32)i / height_segments;

                // Cylinder can be parameterized as follows, where we introduce v
                // parameter that goes in the same direction as the v tex-coord
                // so that the bitangent goes in the same direction as the v tex-coord.
                //   Let r0 be the bottom radius and let r1 be the top radius.
                //   y(v) = h - hv for v in [0,1].
                //   r(v) = r1 + (r0-r1)v
                //
                //   x(t, v) = r(v)*cos(t)
                //   y(t, v) = h - hv
                //   z(t, v) = r(v)*sin(t)
                // 
                //  dx/dt = -r(v)*sin(t)
                //  dy/dt = 0
                //  dz/dt = +r(v)*cos(t)
                //
                //  dx/dv = (r0-r1)*cos(t)
                //  dy/dv = -h
                //  dz/dv = (r0-r1)*sin(t)

                // This is unit length.
                vertex.tangent.x = -s;
                vertex.tangent.y = 0.0f;
                vertex.tangent.z = c;

                f32 dr = bottom_radius - top_radius;
                Vector3 bitangent(dr * c, -height, dr * s);

                Vector3 T = Vector3(vertex.tangent);
                Vector3 B = Vector3(bitangent);
                Vector3 N = T.Cross(B);
                N.Normalize();
                vertex.normal = N;

                data.m_vertices.push_back(vertex);
            }
        }

        // Add one because we duplicate the first and last vertex per ring
        // since the texture coordinates are different.
        u32 ring_vertex_count = radial_segments + 1;

        // Compute indices for each stack.
        for (u32 i = 0; i < height_segments; ++i)
        {
            for (u32 j = 0; j < radial_segments; ++j)
            {
                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j + 1));

                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j + 1));
                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j + 1));
            }
        }
    }

    void build_cylinder_top_cap(
        f32 top_radius, f32 height,
        u32 slice_count,
        PrimitiveMeshGeometryData& data)
    {
        u32 base_index = static_cast<u32>(data.m_vertices.size());

        f32 y = 0.5f * height;
        f32 d_theta = 2.0f * ZV_PI / slice_count;

        // Duplicate cap ring vertices because the texture coordinates and normals differ.
        for (u32 i = 0; i <= slice_count; ++i)
        {
            f32 x = top_radius * ZV::cos(i * d_theta);
            f32 z = top_radius * ZV::sin(i * d_theta);

            // Scale down by the height to try and make top cap texture coord area
            // proportional to base.
            f32 u = x / height + 0.5f;
            f32 v = z / height + 0.5f;

            data.m_vertices.push_back({Vector3(x, y, z), Vector2(u, v), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
        }

        // Cap center vertex.
        data.m_vertices.push_back({Vector3(0.0f, y, 0.0f), Vector2(0.5f, 0.5f), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});

        // Index of center vertex.
        u32 center_index = static_cast<u32>(data.m_vertices.size() - 1);

        for (u32 i = 0; i < slice_count; ++i)
        {
            data.m_indices.push_back(static_cast<u16>(center_index));
            data.m_indices.push_back(static_cast<u16>(base_index + i + 1));
            data.m_indices.push_back(static_cast<u16>(base_index + i));
        }
    }

    void build_cylinder_bottom_cap(
        f32 bottom_radius, f32 height,
        u32 slice_count,
        PrimitiveMeshGeometryData& data)
    {
        // 
        // Build bottom cap.
        //

        u32 base_index = static_cast<u32>(data.m_vertices.size());
        f32 y = -0.5f * height;

        // vertices of ring
        f32 d_theta = 2.0f * ZV_PI / slice_count;
        for (u32 i = 0; i <= slice_count; ++i)
        {
            f32 x = bottom_radius * ZV::cos(i * d_theta);
            f32 z = bottom_radius * ZV::sin(i * d_theta);

            // Scale down by the height to try and make top cap texture coord area
            // proportional to base.
            f32 u = x / height + 0.5f;
            f32 v = z / height + 0.5f;

            data.m_vertices.push_back({Vector3(x, y, z), Vector2(u, v), Vector3(0.0f, -1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
        }

        // Cap center vertex.
        data.m_vertices.push_back({Vector3(0.0f, y, 0.0f), Vector2(0.5f, 0.5f), Vector3(0.0f, -1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});

        // Cache the index of center vertex.
        u32 center_index = static_cast<u32>(data.m_vertices.size() - 1);

        for (u32 i = 0; i < slice_count; ++i)
        {
            data.m_indices.push_back(static_cast<u16>(center_index));
            data.m_indices.push_back(static_cast<u16>(base_index + i));
            data.m_indices.push_back(static_cast<u16>(base_index + i + 1));
        }
    }

    void build_capsule_top_cap(
        f32 radius, f32 height,
        u32 slice_count, u32 stack_count,
        PrimitiveMeshGeometryData& data)
    {
        f32 phi_step = 0.5f * ZV_PI / stack_count;
        f32 theta_step = 2.0f * ZV_PI / slice_count;
    
        // Start from top pole (we won't use it, but we build rings starting just below)
        f32 y_offset = 0.5f * height;
    
        // Build rings (excluding the top pole)
        for (u32 i = 0; i <= stack_count; ++i)
        {
            f32 phi = i * phi_step;
            for (u32 j = 0; j <= slice_count; ++j)
            {
                f32 theta = j * theta_step;
    
                MeshVertex v;
                v.position.x = radius * ZV::sin(phi) * ZV::cos(theta);
                v.position.y = radius * ZV::cos(phi) + y_offset;
                v.position.z = radius * ZV::sin(phi) * ZV::sin(theta);
    
                // Partial derivative of P with respect to theta
                v.tangent.x = -radius * ZV::sin(phi) * ZV::sin(theta);
                v.tangent.y = 0.0f;
                v.tangent.z = +radius * ZV::sin(phi) * ZV::cos(theta);
                v.tangent.w = 1.0f;
                v.tangent.Normalize();

                Vector3 p = v.position - Vector3(0.0f, y_offset, 0.0f);
                v.normal = p;
                v.normal.Normalize();
    
                v.uv.x = theta / ZV_2PI;
                v.uv.y = 1.0f - phi / ZV_PI;
    
                data.m_vertices.push_back(v);
            }
        }
    
        // Indices
        u32 ring_vertex_count = slice_count + 1;
        u32 base_index = static_cast<u32>(data.m_vertices.size()) - (stack_count + 1) * ring_vertex_count;
        for (u32 i = 0; i < stack_count; ++i)
        {
            for (u32 j = 0; j < slice_count; ++j)
            {
                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j + base_index));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j + 1 + base_index));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j + base_index));
                
                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j + base_index));
                data.m_indices.push_back(static_cast<u16>(i * ring_vertex_count + j + 1 + base_index));
                data.m_indices.push_back(static_cast<u16>((i + 1) * ring_vertex_count + j + 1 + base_index));
            }
        }
    }

    void build_capsule_bottom_cap(
        f32 radius, f32 height,
        u32 slice_count, u32 stack_count,
        PrimitiveMeshGeometryData& data)
    {
        f32 phi_step = 0.5f * ZV_PI / stack_count;
        f32 theta_step = 2.0f * ZV_PI / slice_count;
    
        f32 y_offset = -0.5f * height;
    
        u32 base_index = static_cast<u32>(data.m_vertices.size());
    
        for (u32 i = 0; i <= stack_count; ++i)
        {
            f32 phi = i * phi_step + 0.5f * ZV_PI;
            for (u32 j = 0; j <= slice_count; ++j)
            {
                f32 theta = j * theta_step;

                MeshVertex v;
                v.position.x = radius * ZV::sin(phi) * ZV::cos(theta);
                v.position.y = radius * ZV::cos(phi) + y_offset;
                v.position.z = radius * ZV::sin(phi) * ZV::sin(theta);
    
                // Partial derivative of P with respect to theta
                v.tangent.x = -radius * ZV::sin(phi) * ZV::sin(theta);
                v.tangent.y = 0.0f;
                v.tangent.z = +radius * ZV::sin(phi) * ZV::cos(theta);
                v.tangent.w = 1.0f;
                v.tangent.Normalize();

                Vector3 p = v.position - Vector3(0.0f, y_offset, 0.0f);
                v.normal = p;
                v.normal.Normalize();
    
                v.uv.x = theta / ZV_2PI;
                v.uv.y = phi / ZV_PI;
    
                data.m_vertices.push_back(v);
            }
        }
    
        u32 ring_vertex_count = slice_count + 1;
    
        for (u32 i = 0; i < stack_count; ++i)
        {
            for (u32 j = 0; j < slice_count; ++j)
            {
                u32 a = base_index + i * ring_vertex_count + j;
                u32 b = base_index + (i + 1) * ring_vertex_count + j;
                u32 c = base_index + (i + 1) * ring_vertex_count + j + 1;
                u32 d = base_index + i * ring_vertex_count + j + 1;
    
                // Clockwise winding order for bottom cap (facing -Y)
                data.m_indices.push_back(static_cast<u16>(a));
                data.m_indices.push_back(static_cast<u16>(c));
                data.m_indices.push_back(static_cast<u16>(b));
    
                data.m_indices.push_back(static_cast<u16>(a));
                data.m_indices.push_back(static_cast<u16>(d));
                data.m_indices.push_back(static_cast<u16>(c));
            }
        }
    }
}

SharedPtr<PrimitiveMeshGeometryData> create_triangle(Vector3 p1, Vector3 p2, Vector3 p3)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();
    
    data->m_type = PrimitiveMeshGeometryData::Type::Triangle;

    data->m_vertices.push_back({p1, Vector2(0.0f, 1.0f), Vector3(0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    data->m_vertices.push_back({p2, Vector2(0.5f, 0.0f), Vector3(0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    data->m_vertices.push_back({p3, Vector2(1.0f, 1.0f), Vector3(0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    
    data->m_indices.push_back(0);
    data->m_indices.push_back(1);
    data->m_indices.push_back(2);

    recalculate_normals(data->m_vertices.data(), data->m_vertices.size(), data->m_indices.data(), data->m_indices.size());

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_quad(f32 width, f32 height)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Quad;

    data->m_vertices.push_back({Vector3(-width / 2.0f, -height / 2.0f, 0.0f), Vector2(0.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    data->m_vertices.push_back({Vector3(-width / 2.0f, height / 2.0f, 0.0f), Vector2(0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    data->m_vertices.push_back({Vector3(width / 2.0f, height / 2.0f, 0.0f), Vector2(1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
    data->m_vertices.push_back({Vector3(width / 2.0f, -height / 2.0f, 0.0f), Vector2(1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)});

    data->m_indices.push_back(0);
    data->m_indices.push_back(1);
    data->m_indices.push_back(2);
    data->m_indices.push_back(0);
    data->m_indices.push_back(2);
    data->m_indices.push_back(3);

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_plane(f32 width, f32 height, u32 width_segments, u32 height_segments)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Plane;

    u32 m = width_segments * 2;
    u32 n = height_segments * 2;

	u32 vertex_count = m * n;
	u32 face_count   = (m - 1) * (n - 1) * 2;

	//
	// Create the vertices.
	//

	f32 half_width = 0.5f * width;
	f32 half_height = 0.5f * height;

	f32 dx = width / (n - 1);
	f32 dz = height / (m - 1);

	f32 du = 1.0f / (n - 1);
	f32 dv = 1.0f / (m - 1);

	data->m_vertices.resize(vertex_count);
	
    for (u32 i = 0; i < m; ++i)
	{
		f32 z = half_height - i * dz;

		for (u32 j = 0; j < n; ++j)
		{
			f32 x = -half_width + j * dx;

			data->m_vertices[i * n + j].position = Vector3(x, 0.0f, z);
			data->m_vertices[i * n + j].normal   = Vector3(0.0f, 1.0f, 0.0f);
			data->m_vertices[i * n + j].tangent  = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			// Stretch texture over grid.
			data->m_vertices[i * n + j].uv.x = j * du;
			data->m_vertices[i * n + j].uv.y = i * dv;
		}
	}
 
    //
	// Create the indices.
	//

	data->m_indices.resize(face_count * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	u32 k = 0;
	for (u32 i = 0; i < m - 1; ++i)
	{
		for (u32 j = 0; j < n - 1; ++j)
		{
			data->m_indices[k]     = static_cast<u16>(i * n + j);
			data->m_indices[k + 1] = static_cast<u16>(i * n + j + 1);
			data->m_indices[k + 2] = static_cast<u16>((i + 1) * n + j);

			data->m_indices[k + 3] = static_cast<u16>((i + 1) * n + j);
			data->m_indices[k + 4] = static_cast<u16>(i * n + j + 1);
			data->m_indices[k + 5] = static_cast<u16>((i + 1) * n + j + 1);

			k += 6; // next quad
		}
	}

    recalculate_normals(data->m_vertices.data(), data->m_vertices.size(), data->m_indices.data(), data->m_indices.size());

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_box(f32 width, f32 height, f32 depth, u32 num_subdivisions)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Box;

	MeshVertex v[24];

	f32 w2 = 0.5f * width;
	f32 h2 = 0.5f * height;
	f32 d2 = 0.5f * depth;
    
	// Fill in the front face vertex data.
	v[0] = {Vector3(-w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[1] = {Vector3(-w2, +h2, -d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[2] = {Vector3(+w2, +h2, -d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[3] = {Vector3(+w2, -h2, -d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};

	// Fill in the back face vertex data.
	v[4] = {Vector3(-w2, -h2, +d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[5] = {Vector3(+w2, -h2, +d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[6] = {Vector3(+w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[7] = {Vector3(-w2, +h2, +d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};

	// Fill in the top face vertex data.
	v[8]  = {Vector3(-w2, +h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[9]  = {Vector3(-w2, +h2, +d2), Vector2(0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[10] = {Vector3(+w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};
	v[11] = {Vector3(+w2, +h2, -d2), Vector2(1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)};

	// Fill in the bottom face vertex data.
	v[12] = {Vector3(-w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[13] = {Vector3(+w2, -h2, -d2), Vector2(0.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[14] = {Vector3(+w2, -h2, +d2), Vector2(1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};
	v[15] = {Vector3(-w2, -h2, +d2), Vector2(1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector4(-1.0f, 0.0f, 0.0f, 1.0f)};

	// Fill in the left face vertex data.
	v[16] = {Vector3(-w2, -h2, +d2), Vector2(0.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, -1.0f, 1.0f)};
	v[17] = {Vector3(-w2, +h2, +d2), Vector2(0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, -1.0f, 1.0f)};
	v[18] = {Vector3(-w2, +h2, -d2), Vector2(1.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, -1.0f, 1.0f)};
	v[19] = {Vector3(-w2, -h2, -d2), Vector2(1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, -1.0f, 1.0f)};

	// Fill in the right face vertex data.
	v[20] = {Vector3(+w2, -h2, -d2), Vector2(0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)};
	v[21] = {Vector3(+w2, +h2, -d2), Vector2(0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)};
	v[22] = {Vector3(+w2, +h2, +d2), Vector2(1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)};
	v[23] = {Vector3(+w2, -h2, +d2), Vector2(1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)};

	data->m_vertices.assign(&v[0], &v[24]);
 
	//
	// Create the indices.
	//

	u16 indices[36];

	// Fill in the front face index data
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 0; indices[4] = 2; indices[5] = 3;

	// Fill in the back face index data
	indices[6] = 4; indices[7]  = 5; indices[8]  = 6;
	indices[9] = 4; indices[10] = 6; indices[11] = 7;

	// Fill in the top face index data
	indices[12] = 8; indices[13] =  9; indices[14] = 10;
	indices[15] = 8; indices[16] = 10; indices[17] = 11;

	// Fill in the bottom face index data
	indices[18] = 12; indices[19] = 13; indices[20] = 14;
	indices[21] = 12; indices[22] = 14; indices[23] = 15;

	// Fill in the left face index data
	indices[24] = 16; indices[25] = 17; indices[26] = 18;
	indices[27] = 16; indices[28] = 18; indices[29] = 19;

	// Fill in the right face index data
	indices[30] = 20; indices[31] = 21; indices[32] = 22;
	indices[33] = 20; indices[34] = 22; indices[35] = 23;

	data->m_indices.assign(&indices[0], &indices[36]);

    // Put a cap on the number of subdivisions.
    // num_subdivisions = ZV::min(num_subdivisions, 6u);

    for (u32 i = 0; i < num_subdivisions; ++i)
    {
        subdivide(*data.get());
    }

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_sphere(f32 radius, u32 width_segments, u32 height_segments)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Sphere;

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	MeshVertex top_vertex = {Vector3(0.0f, +radius, 0.0f), Vector2(0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)};
	MeshVertex bottom_vertex = {Vector3(0.0f, -radius, 0.0f), Vector2(0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)};

	data->m_vertices.push_back(top_vertex);

    f32 phi_step   = ZV_PI / height_segments;
    float theta_step = 2.0f * ZV_PI / width_segments;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (u32 i = 1; i <= height_segments - 1; ++i)
	{
		f32 phi = i * phi_step;

		// Vertices of ring.
        for(u32 j = 0; j <= width_segments; ++j)
		{
			f32 theta = j * theta_step;

			MeshVertex v;

			// spherical to cartesian
			v.position.x = radius * ZV::sin(phi) * ZV::cos(theta);
			v.position.y = radius * ZV::cos(phi);
			v.position.z = radius * ZV::sin(phi) * ZV::sin(theta);

			// Partial derivative of P with respect to theta
			v.tangent.x = -radius * ZV::sin(phi) * ZV::sin(theta);
			v.tangent.y = 0.0f;
			v.tangent.z = +radius * ZV::sin(phi) * ZV::cos(theta);
            v.tangent.w = 1.0f;
			v.tangent.Normalize();

            Vector3 p = v.position;
			v.normal = p;
			v.normal.Normalize();

			v.uv.x = theta / ZV_2PI;
			v.uv.y = phi / ZV_PI;

			data->m_vertices.push_back(v);
		}
	}

	data->m_vertices.push_back(bottom_vertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

    for (u32 i = 1; i <= width_segments; ++i)
	{
		data->m_indices.push_back(static_cast<u16>(0));
		data->m_indices.push_back(static_cast<u16>(i + 1));
		data->m_indices.push_back(static_cast<u16>(i));
	}
	
	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
    u32 base_index = 1;
    u32 ring_vertex_count = width_segments + 1;
	for (u32 i = 0; i < height_segments-2; ++i)
	{
		for (u32 j = 0; j < width_segments; ++j)
		{
			data->m_indices.push_back(static_cast<u16>(base_index + i * ring_vertex_count + j));
			data->m_indices.push_back(static_cast<u16>(base_index + i * ring_vertex_count + j + 1));
			data->m_indices.push_back(static_cast<u16>(base_index + (i + 1) * ring_vertex_count + j));

			data->m_indices.push_back(static_cast<u16>(base_index + (i + 1) * ring_vertex_count + j));
			data->m_indices.push_back(static_cast<u16>(base_index + i * ring_vertex_count + j + 1));
			data->m_indices.push_back(static_cast<u16>(base_index + (i + 1) * ring_vertex_count + j+1));
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	u32 south_pole_index = static_cast<u32>(data->m_vertices.size()-1);

	// Offset the indices to the index of the first vertex in the last ring.
	base_index = south_pole_index - ring_vertex_count;
	
	for (u32 i = 0; i < width_segments; ++i)
	{
		data->m_indices.push_back(static_cast<u16>(south_pole_index));
		data->m_indices.push_back(static_cast<u16>(base_index + i));
		data->m_indices.push_back(static_cast<u16>(base_index + i + 1));
	}

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_icosphere(f32 radius, u32 num_subdivisions)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Icosphere;

    // Put a cap on the number of subdivisions.
    // num_subdivisions = ZV::min<u32>(num_subdivisions, 6u);

    // Approximate a sphere by tessellating an icosahedron.

    const f32 X = 0.525731f; 
    const f32 Z = 0.850651f;

    Vector3 pos[12] = 
    {
        Vector3(-X, 0.0f, Z),  Vector3(X, 0.0f, Z),  
        Vector3(-X, 0.0f, -Z), Vector3(X, 0.0f, -Z),    
        Vector3(0.0f, Z, X),   Vector3(0.0f, Z, -X), 
        Vector3(0.0f, -Z, X),  Vector3(0.0f, -Z, -X),    
        Vector3(Z, X, 0.0f),   Vector3(-Z, X, 0.0f), 
        Vector3(Z, -X, 0.0f),  Vector3(-Z, -X, 0.0f)
    };

    u16 k[60] =
    {
        1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,    
        1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,    
        3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0, 
        10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7 
    };

    data->m_vertices.resize(12);
    data->m_indices.assign(&k[0], &k[60]);

    for (u32 i = 0; i < 12; ++i)
    {
        data->m_vertices[i].position = pos[i];
    }

    for (u32 i = 0; i < num_subdivisions; ++i)
    {
        subdivide(*data.get());
    }

    // Project vertices onto sphere and scale.
    for (u32 i = 0; i < data->m_vertices.size(); ++i)
    {
        // Project onto unit sphere.
        Vector3 n = Vector3(data->m_vertices[i].position);
        n.Normalize();

        // Project onto sphere.
        Vector3 p = radius * n;

        data->m_vertices[i].position = p;
        data->m_vertices[i].normal = n;

        // Derive texture coordinates from spherical coordinates.
        f32 theta = atan2f(data->m_vertices[i].position.z, data->m_vertices[i].position.x);

        // Put in [0, 2pi].
        if (theta < 0.0f)
        {
            theta += ZV_2PI;
        }

        f32 phi = acosf(data->m_vertices[i].position.y / radius);

        data->m_vertices[i].uv.x = theta / ZV_2PI;
        data->m_vertices[i].uv.y = phi / ZV_PI;

        // Partial derivative of P with respect to theta
        data->m_vertices[i].tangent.x = -radius*ZV::sin(phi)*ZV::sin(theta);
        data->m_vertices[i].tangent.y = 0.0f;
        data->m_vertices[i].tangent.z = +radius*ZV::sin(phi)*ZV::cos(theta);
        data->m_vertices[i].tangent.w = 1.0f;
        data->m_vertices[i].tangent.Normalize();
    }

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_cylinder(f32 bottom_radius, f32 top_radius, f32 height, u32 radial_segments, u32 height_segments)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Cylinder;

    build_cylinder_side(bottom_radius, top_radius, height, radial_segments, height_segments, *data.get());

	build_cylinder_top_cap(top_radius, height, radial_segments, *data.get());
	build_cylinder_bottom_cap(bottom_radius, height, radial_segments, *data.get());

    return data;
}

SharedPtr<PrimitiveMeshGeometryData> create_capsule(f32 radius, f32 height, u32 radial_segments, u32 height_segments, u32 cap_segments)
{
    SharedPtr<PrimitiveMeshGeometryData> data = make_shared_ptr<PrimitiveMeshGeometryData>();

    data->m_type = PrimitiveMeshGeometryData::Type::Capsule;

    build_cylinder_side(radius, radius, height, radial_segments, height_segments, *data.get());

    build_capsule_top_cap(radius, height, radial_segments, cap_segments, *data.get());
    build_capsule_bottom_cap(radius, height, radial_segments, cap_segments, *data.get());

    return data;
}
