import bpy
from math import radians
from pathlib import Path

SOURCE_DIR = Path(r"C:\Users\k024g\OneDrive\デスクトップ\thruster")
OBJ_PATH = SOURCE_DIR / "thruster.obj"
OUTPUT_PATH = Path(
    r"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2"
    r"\project\resources\vf-15c\vf15c_thruster_preview.blend"
)


def import_thruster(name: str, location: tuple[float, float, float]):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.obj_import(filepath=str(OBJ_PATH))
    thruster = bpy.context.selected_objects[0]
    thruster.name = name
    thruster.data.name = "ThrusterMesh"
    thruster.location = location
    thruster.rotation_euler = (radians(90), 0.0, 0.0)
    return thruster


# Match the intended rear mounting point from the reference Blender scene.
left = import_thruster("Thruster_Left", (-0.74827, 10.758, 7.975))

# Duplicate the same mesh for a matched right-hand nozzle.
right = left.copy()
right.data = left.data.copy()
right.name = "Thruster_Right"
right.data.name = "ThrusterMesh_Right"
right.location.x = 0.74827
for collection in left.users_collection:
    collection.objects.link(right)

bpy.context.view_layer.objects.active = left
left.select_set(True)
right.select_set(True)

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_PATH))
print(f"Saved {OUTPUT_PATH}")
