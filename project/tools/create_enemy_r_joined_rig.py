"""Rebind the joined EnemyR OBJ to the existing EnemyRig armature.

The supplied OBJ combines each leg's previously separate rigid pieces.  The
original EnemyR_Rig.blend still has those pieces and their single-bone vertex
groups, so their world-space vertices identify each imported part precisely.
Each combined leg is then deliberately bound 100% to its hip bone, keeping the
whole leg rigid while it swings and preventing the old part-separation issue.
"""

from pathlib import Path

import bpy
from mathutils.kdtree import KDTree


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_BLEND = PROJECT_ROOT / "resources" / "EnemyR_Rig" / "EnemyR_Rig.blend"
SOURCE_OBJ = Path(r"C:\Users\k024g\OneDrive\デスクトップ\EnemyR.obj")
OUTPUT_DIRECTORY = PROJECT_ROOT / "resources" / "EnemyR_Joined_Rig"
OUTPUT_BLEND = OUTPUT_DIRECTORY / "EnemyR_Joined_Rig.blend"
OUTPUT_GLTF = OUTPUT_DIRECTORY / "EnemyR_Joined_Rig.gltf"


def group_name_for_vertex(mesh_object, vertex):
    groups_by_index = {group.index: group.name for group in mesh_object.vertex_groups}
    assignments = [assignment for assignment in vertex.groups if assignment.weight > 0.0]
    if not assignments:
        return None
    strongest = max(assignments, key=lambda assignment: assignment.weight)
    return groups_by_index.get(strongest.group)


def main():
    if not SOURCE_BLEND.is_file():
        raise RuntimeError(f"Rig source blend was not found: {SOURCE_BLEND}")
    if not SOURCE_OBJ.is_file():
        raise RuntimeError(f"Joined EnemyR OBJ was not found: {SOURCE_OBJ}")

    bpy.ops.wm.open_mainfile(filepath=str(SOURCE_BLEND))

    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    if len(armatures) != 1:
        raise RuntimeError(f"Expected one armature, found {len(armatures)}")
    rig = armatures[0]
    original_meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]

    # Make a nearest-vertex lookup from the original, rigidly weighted pieces.
    source_vertices = []
    for mesh_object in original_meshes:
        for vertex in mesh_object.data.vertices:
            bone_name = group_name_for_vertex(mesh_object, vertex)
            if bone_name:
                source_vertices.append((mesh_object.matrix_world @ vertex.co, bone_name))
    if not source_vertices:
        raise RuntimeError("The original rig has no weighted mesh vertices")

    lookup = KDTree(len(source_vertices))
    for index, (position, _bone_name) in enumerate(source_vertices):
        lookup.insert(position, index)
    lookup.balance()

    existing_object_ids = {obj.as_pointer() for obj in bpy.data.objects}
    bpy.ops.wm.obj_import(filepath=str(SOURCE_OBJ))
    joined_meshes = [
        obj
        for obj in bpy.context.scene.objects
        if obj.type == "MESH" and obj.as_pointer() not in existing_object_ids
    ]
    if not joined_meshes:
        raise RuntimeError("No meshes were imported from the joined EnemyR OBJ")

    max_distance = 0.0
    transferred_vertices = 0
    bone_names = {bone.name for bone in rig.data.bones}
    for mesh_object in joined_meshes:
        # The importer can retain groups when re-running the tool; replace them
        # so the exported skin has exactly one rigid influence per vertex.
        mesh_object.vertex_groups.clear()
        groups = {}
        transferred_bones = set()
        for vertex in mesh_object.data.vertices:
            world_position = mesh_object.matrix_world @ vertex.co
            _nearest, source_index, distance = lookup.find(world_position)
            max_distance = max(max_distance, distance)
            bone_name = source_vertices[source_index][1]
            if bone_name not in bone_names:
                raise RuntimeError(f"Unknown transferred bone: {bone_name}")
            if bone_name not in groups:
                groups[bone_name] = mesh_object.vertex_groups.new(name=bone_name)
            group = groups[bone_name]
            group.add([vertex.index], 1.0, "REPLACE")
            transferred_bones.add(bone_name)
            transferred_vertices += 1

        # The four joined legs used to contain independent hip, knee and toe
        # objects.  They now form one rigid unit, so bind every vertex in that
        # unit to its hip.  Child bones are kept in the armature for compatible
        # poses, but cannot pull the formerly separate visual parts apart.
        hip_bones = [bone_name for bone_name in transferred_bones if bone_name.endswith("_Hip")]
        if len(mesh_object.data.vertices) >= 470 and len(hip_bones) == 1:
            mesh_object.vertex_groups.clear()
            hip_group = mesh_object.vertex_groups.new(name=hip_bones[0])
            hip_group.add([vertex.index for vertex in mesh_object.data.vertices], 1.0, "REPLACE")

        mesh_object.parent = rig
        modifier = mesh_object.modifiers.new(name="EnemyRigSkin", type="ARMATURE")
        modifier.object = rig
        modifier.use_deform_preserve_volume = True

    # The joined OBJ must be the only visible/rendered model.  Keeping the
    # armature preserves the original rest pose and all existing bone names.
    for mesh_object in original_meshes:
        bpy.data.objects.remove(mesh_object, do_unlink=True)

    if max_distance > 0.0001:
        raise RuntimeError(
            f"Vertex transfer did not match the source geometry (max distance: {max_distance:.6f})"
        )

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    for mesh_object in joined_meshes:
        mesh_object.select_set(True)
    bpy.context.view_layer.objects.active = rig

    # This output is reproducible from the two source assets.  Avoid leaving
    # Blender's numbered backup beside a runtime resource on subsequent runs.
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND))
    bpy.ops.export_scene.gltf(
        filepath=str(OUTPUT_GLTF),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_animations=False,
        export_yup=True,
    )
    print(
        f"Created {OUTPUT_GLTF} with {len(joined_meshes)} meshes, "
        f"{transferred_vertices} weighted vertices, and max transfer distance {max_distance:.8f}"
    )


if __name__ == "__main__":
    main()
