#include "engine/graphics/meshes/terrain_mesh.h"
#include <iostream>
#include <stdexcept>
void Check(const bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
class Floor final : public WorldGenerator
{
    void Generate(Chunk& c) const override
    { for(int z=0;z<16;++z) for(int x=0;x<16;++x) c.SetBlock(x,0,z,1); }
};
int main()
{
    try
    {
        World world(std::make_unique<Floor>(),1); world.Update(0,0,9);
        TerrainMeshCache cache;
        for(int i=0;i<20;++i) cache.Get(world,{0,3,0});
        auto unaffected=cache.Get(world,{0,3,0}).at({1,1}).generation;
        auto changed=cache.Get(world,{0,3,0}).at({0,0}).generation;
        // Multiple changes before a render must preserve unrelated cache entries.
        world.SetBlock(0,1,0,1); world.SetBlock(1,1,0,1);
        for(int i=0;i<20;++i) cache.Get(world,{0,3,0});
        const auto& entries=cache.Get(world,{0,3,0});
        Check(entries.at({1,1}).generation==unaffected,"Unrelated mesh survives skipped revisions");
        Check(entries.at({0,0}).generation!=changed,"Changed chunk rebuilt after skipped revisions");
        auto before=entries.at({0,0}).generation;
        world.Update(1000,1000,9); world.Update(0,0,9);
        for(int i=0;i<20;++i) cache.Get(world,{0,3,0});
        Check(cache.Get(world,{0,3,0}).at({0,0}).generation!=before,"Reload identity invalidates cache even when content version repeats");
        std::cout<<"Terrain cache checks passed\n";
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
