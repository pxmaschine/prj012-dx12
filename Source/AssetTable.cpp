
struct TextureLoadInfo
{
    const char* m_path;
    TextureFormat m_format = TextureFormat::SRGB;
    s32 m_request_channels = 4;
    // TODO: This is not nice!!!
    ChannelPackingFlags m_channel_packing{ ChannelPacking::None };
};

enum class ModelFormat : u8
{
    GLTF,
};

struct ModelLoadInfo
{
    const char* m_path;
    ModelFormat m_format = ModelFormat::GLTF;
};

static HashMap<AssetId, TextureLoadInfo> s_texture_load_infos = 
{
    {AssetId("dummy"), TextureLoadInfo{"Assets/Textures/dummy.png", TextureFormat::SRGB, 3}},

    // Protogrid
    {AssetId("grid_albedo"),     TextureLoadInfo{"Assets/Textures/protogrid/T_Paint_Diffuse.png",    TextureFormat::SRGB, 4}},
    {AssetId("grid_overlay"),    TextureLoadInfo{"Assets/Textures/protogrid/T_Paint_Grid.png",       TextureFormat::SRGB, 4}},
    {AssetId("grid_normal"),     TextureLoadInfo{"Assets/Textures/protogrid/T_Paint_Normal.png",     TextureFormat::Linear, 4}},
    {AssetId("grid_roughness"),  TextureLoadInfo{"Assets/Textures/protogrid/T_Paint_Glossiness.png", TextureFormat::Linear, 4}},

    // Brick wall (low)
    {AssetId("brick_wall_low_albedo"), TextureLoadInfo{"Assets/Textures/brickwall.jpg", TextureFormat::SRGB, 4}},
    {AssetId("brick_wall_low_normal"), TextureLoadInfo{"Assets/Textures/brickwall_normal.jpg", TextureFormat::Linear, 4}},

    // Brick wall (high)
    {AssetId("brick_wall_albedo"),    TextureLoadInfo{"Assets/Textures/rough_brick_wall_th5mdawaw_4k/Rough_Brick_Wall_th5mdawaw_4K_BaseColor.jpg", TextureFormat::SRGB, 4}},
    {AssetId("brick_wall_normal"),    TextureLoadInfo{"Assets/Textures/rough_brick_wall_th5mdawaw_4k/Rough_Brick_Wall_th5mdawaw_4K_Normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("brick_wall_roughness"), TextureLoadInfo{"Assets/Textures/rough_brick_wall_th5mdawaw_4k/Rough_Brick_Wall_th5mdawaw_4K_Roughness.jpg", TextureFormat::Linear, 4}},
    {AssetId("brick_wall_specular"),  TextureLoadInfo{"Assets/Textures/rough_brick_wall_th5mdawaw_4k/Rough_Brick_Wall_th5mdawaw_4K_Specular.jpg", TextureFormat::Linear, 1}},
    {AssetId("brick_wall_ao"),        TextureLoadInfo{"Assets/Textures/rough_brick_wall_th5mdawaw_4k/Rough_Brick_Wall_th5mdawaw_4K_AO.jpg", TextureFormat::Linear, 1}},

    // Metal sheet
    {AssetId("metal_sheet_albedo"),    TextureLoadInfo{"Assets/Textures/corrugated_metal_sheet_teendf3q_4k/Corrugated_Metal_Sheet_teendf3q_4K_BaseColor.jpg", TextureFormat::SRGB, 4}},
    {AssetId("metal_sheet_normal"),    TextureLoadInfo{"Assets/Textures/corrugated_metal_sheet_teendf3q_4k/Corrugated_Metal_Sheet_teendf3q_4K_Normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("metal_sheet_metalRoughness"), TextureLoadInfo{"Assets/Textures/corrugated_metal_sheet_teendf3q_4k/Corrugated_Metal_Sheet_teendf3q_4K_MetalnessRoughness.jpg", TextureFormat::Linear, 4}},
    {AssetId("metal_sheet_specular"),  TextureLoadInfo{"Assets/Textures/corrugated_metal_sheet_teendf3q_4k/Corrugated_Metal_Sheet_teendf3q_4K_Specular.jpg", TextureFormat::Linear, 1}},
    {AssetId("metal_sheet_ao"),        TextureLoadInfo{"Assets/Textures/corrugated_metal_sheet_teendf3q_4k/Corrugated_Metal_Sheet_teendf3q_4K_AO.jpg", TextureFormat::Linear, 1}},

    // Planks
    {AssetId("planks_albedo"),    TextureLoadInfo{"Assets/Textures/old_worn_planks_tijlbc1aw_4k/Old_Worn_Planks_tijlbc1aw_4K_BaseColor.jpg", TextureFormat::SRGB, 4}},
    {AssetId("planks_normal"),    TextureLoadInfo{"Assets/Textures/old_worn_planks_tijlbc1aw_4k/Old_Worn_Planks_tijlbc1aw_4K_Normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("planks_roughness"), TextureLoadInfo{"Assets/Textures/old_worn_planks_tijlbc1aw_4k/Old_Worn_Planks_tijlbc1aw_4K_Roughness.jpg", TextureFormat::Linear, 4}},
    {AssetId("planks_specular"),  TextureLoadInfo{"Assets/Textures/old_worn_planks_tijlbc1aw_4k/Old_Worn_Planks_tijlbc1aw_4K_Specular.jpg", TextureFormat::Linear, 1}},
    {AssetId("planks_ao"),        TextureLoadInfo{"Assets/Textures/old_worn_planks_tijlbc1aw_4k/Old_Worn_Planks_tijlbc1aw_4K_AO.jpg", TextureFormat::Linear, 1}},

    // Tiles
    {AssetId("tiles_albedo"),    TextureLoadInfo{"Assets/Textures/shiny_worn_shower_tiles_tbskcjdr_4k/Shiny_Worn_Shower_Tiles_tbskcjdr_4K_BaseColor.jpg", TextureFormat::SRGB, 4}},
    {AssetId("tiles_normal"),    TextureLoadInfo{"Assets/Textures/shiny_worn_shower_tiles_tbskcjdr_4k/Shiny_Worn_Shower_Tiles_tbskcjdr_4K_Normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("tiles_roughness"), TextureLoadInfo{"Assets/Textures/shiny_worn_shower_tiles_tbskcjdr_4k/Shiny_Worn_Shower_Tiles_tbskcjdr_4K_Roughness.jpg", TextureFormat::Linear, 4}},
    {AssetId("tiles_specular"),  TextureLoadInfo{"Assets/Textures/shiny_worn_shower_tiles_tbskcjdr_4k/Shiny_Worn_Shower_Tiles_tbskcjdr_4K_Specular.jpg", TextureFormat::Linear, 1}},
    {AssetId("tiles_ao"),        TextureLoadInfo{"Assets/Textures/shiny_worn_shower_tiles_tbskcjdr_4k/Shiny_Worn_Shower_Tiles_tbskcjdr_4K_AO.jpg", TextureFormat::Linear, 1}},

    // Wood
    {AssetId("wood_albedo"),    TextureLoadInfo{"Assets/Textures/varnished_wood_planks_tifleisfw_4k/Varnished_Wood_Planks_tifleisfw_4K_BaseColor.jpg", TextureFormat::SRGB, 4}},
    {AssetId("wood_normal"),    TextureLoadInfo{"Assets/Textures/varnished_wood_planks_tifleisfw_4k/Varnished_Wood_Planks_tifleisfw_4K_Normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("wood_roughness"), TextureLoadInfo{"Assets/Textures/varnished_wood_planks_tifleisfw_4k/Varnished_Wood_Planks_tifleisfw_4K_Roughness.jpg", TextureFormat::Linear, 4}},
    {AssetId("wood_specular"),  TextureLoadInfo{"Assets/Textures/varnished_wood_planks_tifleisfw_4k/Varnished_Wood_Planks_tifleisfw_4K_Specular.jpg", TextureFormat::Linear, 1}},
    {AssetId("wood_ao"),        TextureLoadInfo{"Assets/Textures/varnished_wood_planks_tifleisfw_4k/Varnished_Wood_Planks_tifleisfw_4K_AO.jpg", TextureFormat::Linear, 1}},

    // Damaged Helmet
    {AssetId("DamagedHelmet/Default_albedo.jpg"), TextureLoadInfo{"Assets/Textures/Models/DamagedHelmet/Default_albedo.jpg", TextureFormat::SRGB, 4}},
    {AssetId("DamagedHelmet/Default_normal.jpg"), TextureLoadInfo{"Assets/Textures/Models/DamagedHelmet/Default_normal.jpg", TextureFormat::Linear, 4}},
    {AssetId("DamagedHelmet/Default_metalRoughness.jpg"), TextureLoadInfo{"Assets/Textures/Models/DamagedHelmet/Default_metalRoughness.jpg", TextureFormat::Linear, 4, ChannelPacking::Metalness | ChannelPacking::Roughness}},
    {AssetId("DamagedHelmet/Default_AO.jpg"), TextureLoadInfo{"Assets/Textures/Models/DamagedHelmet/Default_AO.jpg", TextureFormat::Linear, 1}},
    {AssetId("DamagedHelmet/Default_emissive.jpg"), TextureLoadInfo{"Assets/Textures/Models/DamagedHelmet/Default_emissive.jpg", TextureFormat::SRGB, 4}},

    // Sponza
    {AssetId("Sponza/white.png"), TextureLoadInfo{"Assets/Textures/Models/Sponza/white.png", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/332936164838540657.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/332936164838540657.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/466164707995436622.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/466164707995436622.jpg", TextureFormat::Linear, 4, ChannelPacking::Metalness | ChannelPacking::Roughness}},
    {AssetId("Sponza/715093869573992647.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/715093869573992647.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/755318871556304029.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/755318871556304029.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/759203620573749278.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/759203620573749278.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/10381718147657362067.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/10381718147657362067.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/10388182081421875623.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/10388182081421875623.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/11474523244911310074.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/11474523244911310074.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/11490520546946913238.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/11490520546946913238.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/11872827283454512094.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/11872827283454512094.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/11968150294050148237.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/11968150294050148237.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/1219024358953944284.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/1219024358953944284.jpg", TextureFormat::Linear, 4, ChannelPacking::Metalness | ChannelPacking::Roughness}},
    {AssetId("Sponza/12501374198249454378.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/12501374198249454378.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/13196865903111448057.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/13196865903111448057.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/13824894030729245199.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/13824894030729245199.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/13982482287905699490.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/13982482287905699490.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/14118779221266351425.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/14118779221266351425.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/14170708867020035030.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/14170708867020035030.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/14267839433702832875.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/14267839433702832875.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/14650633544276105767.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/14650633544276105767.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/15295713303328085182.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/15295713303328085182.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/15722799267630235092.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/15722799267630235092.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/16275776544635328252.png"), TextureLoadInfo{"Assets/Textures/Models/Sponza/16275776544635328252.png", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/16299174074766089871.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/16299174074766089871.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/16885566240357350108.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/16885566240357350108.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/17556969131407844942.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/17556969131407844942.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/17876391417123941155.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/17876391417123941155.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/2051777328469649772.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2051777328469649772.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/2185409758123873465.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2185409758123873465.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/2299742237651021498.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2299742237651021498.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/2374361008830720677.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2374361008830720677.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/2411100444841994089.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2411100444841994089.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/2775690330959970771.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2775690330959970771.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/2969916736137545357.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/2969916736137545357.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/3371964815757888145.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/3371964815757888145.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/3455394979645218238.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/3455394979645218238.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/3628158980083700836.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/3628158980083700836.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/3827035219084910048.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/3827035219084910048.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/4477655471536070370.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4477655471536070370.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/4601176305987539675.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4601176305987539675.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/4675343432951571524.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4675343432951571524.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/4871783166746854860.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4871783166746854860.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/4910669866631290573.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4910669866631290573.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/4975155472559461469.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/4975155472559461469.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/5061699253647017043.png"), TextureLoadInfo{"Assets/Textures/Models/Sponza/5061699253647017043.png", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/5792855332885324923.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/5792855332885324923.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/5823059166183034438.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/5823059166183034438.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/6047387724914829168.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/6047387724914829168.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/6151467286084645207.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/6151467286084645207.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/6593109234861095314.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/6593109234861095314.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/6667038893015345571.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/6667038893015345571.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/6772804448157695701.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/6772804448157695701.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/7056944414013900257.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/7056944414013900257.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/7268504077753552595.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/7268504077753552595.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/7441062115984513793.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/7441062115984513793.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/7645212358685992005.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/7645212358685992005.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/7815564343179553343.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/7815564343179553343.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness | ChannelPacking::Metalness}},
    {AssetId("Sponza/8006627369776289000.png"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8006627369776289000.png", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/8051790464816141987.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8051790464816141987.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/8114461559286000061.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8114461559286000061.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/8481240838833932244.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8481240838833932244.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/8503262930880235456.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8503262930880235456.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/8747919177698443163.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8747919177698443163.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/8750083169368950601.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8750083169368950601.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/8773302468495022225.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8773302468495022225.jpg", TextureFormat::Linear, 4}},
    {AssetId("Sponza/8783994986360286082.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/8783994986360286082.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
    {AssetId("Sponza/9288698199695299068.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/9288698199695299068.jpg", TextureFormat::SRGB, 4}},
    {AssetId("Sponza/9916269861720640319.jpg"), TextureLoadInfo{"Assets/Textures/Models/Sponza/9916269861720640319.jpg", TextureFormat::Linear, 4, ChannelPacking::Roughness}},
};

static HashMap<AssetId, ModelLoadInfo> s_model_load_infos = 
{
    {AssetId("DamagedHelmet"), ModelLoadInfo{"Assets/Models/DamagedHelmet/DamagedHelmet.gltf", ModelFormat::GLTF}},
    {AssetId("Sponza"), ModelLoadInfo{"Assets/Models/Sponza/Sponza.gltf", ModelFormat::GLTF}},
};
