import surface_material
import volume_material
import texture
from taichi.dynamics import levelset

def asset_ptr_to_id(d):
    classes = [surface_material.SurfaceMaterial, volume_material.VolumeMaterial, texture.Texture,
               levelset.LevelSet]
    for key in d:
        for c in classes:
            if isinstance(d[key], c):
                d[key] = d[key].id
    return d
