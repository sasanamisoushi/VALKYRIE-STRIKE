"""Build a rigid, bone-parented glTF version of EnemyR for GroundEnemy.

Run with Blender in background mode.  The source OBJ is intentionally left untouched.
"""

from pathlib import Path
import bpy
from mathutils import Quaternion, Vector


ROOT = Path(__file__).resolve().parent.parent
SOURCE_OBJ = ROOT / "resources" / "EnemyR.obj"
OUTPUT_DIR = ROOT / "resources" / "EnemyR_Rig"
OUTPUT_BLEND = OUTPUT_DIR / "EnemyR_Rig.blend"
OUTPUT_GLTF = OUTPUT_DIR / "EnemyR_Rig.gltf"


def model_point(x, y, z):
    """Convert EnemyR's OBJ coordinates (Y-up) to Blender's imported space (Z-up)."""
    return Vector((x, -z, y))


def source_point(point):
    """Convert a Blender world point back to EnemyR's OBJ coordinate space."""
    return Vector((point.x, point.z, -point.y))


# The OBJ is authored with X as the animal's length and Y as up.  Each tuple is
# hip, knee, ankle and toe in the model's bind pose. Blender imports it as Z-up,
# so every coordinate is converted before armature creation.
LEGS = (
    # 手入力で EnemyR の上脚・膝・足首・足先の接続中心へ合わせた座標。
    # 前脚の根元は従来の自動推定より外側にあり、0.65 ではなく 0.95 が正しい。
    ("FrontRight", model_point(0.95, 1.51, 0.75), model_point(0.65, 0.49, 0.75), model_point(1.28, 0.11, 0.75), model_point(1.48, 0.02, 0.75)),
    ("FrontLeft", model_point(0.95, 1.51, -0.75), model_point(0.65, 0.49, -0.75), model_point(1.28, 0.11, -0.75), model_point(1.48, 0.02, -0.75)),
    ("RearLeft", model_point(-0.72, 1.51, -0.75), model_point(-0.90, 0.49, -0.75), model_point(-0.17, 0.11, -0.75), model_point(-0.04, 0.02, -0.75)),
    ("RearRight", model_point(-0.72, 1.51, 0.75), model_point(-0.90, 0.49, 0.75), model_point(-0.17, 0.11, 0.75), model_point(-0.04, 0.02, 0.75)),
)


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def bounds_center(obj):
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return sum(points, Vector()) / len(points)


def bounds_size(obj):
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    minimum = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
    maximum = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
    return maximum - minimum


def add_bone(edit_bones, name, head, tail, parent=None):
    bone = edit_bones.new(name)
    bone.head = head
    bone.tail = tail
    bone.parent = parent
    bone.use_connect = False
    return bone


def create_armature():
    bpy.ops.object.armature_add(enter_editmode=True, location=(0.0, 0.0, 0.0))
    armature = bpy.context.object
    armature.name = "EnemyRig"
    armature.data.name = "EnemyRig"
    armature.show_in_front = True

    edit_bones = armature.data.edit_bones
    root = edit_bones[0]
    root.name = "Body"
    root.head = model_point(0.0, 1.12, 0.0)
    root.tail = model_point(0.0, 2.35, 0.0)

    for leg_name, hip, knee, ankle, toe in LEGS:
        hip_bone = add_bone(edit_bones, f"{leg_name}_Hip", hip, knee, root)
        knee_bone = add_bone(edit_bones, f"{leg_name}_Knee", knee, ankle, hip_bone)
        ankle_bone = add_bone(edit_bones, f"{leg_name}_Ankle", ankle, toe, knee_bone)
        # A short toe bone gives the claw/foot plates a stable final parent.
        toe_tail = toe + Vector((0.22 if toe.x >= 0.0 else -0.22, 0.0, 0.0))
        add_bone(edit_bones, f"{leg_name}_Toe", toe, toe_tail, ankle_bone)

    turret = add_bone(edit_bones, "Turret", model_point(0.0, 1.78, 0.0), model_point(0.0, 2.45, 0.0), root)
    add_bone(edit_bones, "TurretMuzzle", model_point(0.0, 2.18, 0.0), model_point(1.15, 2.18, 0.0), turret)
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def choose_bone_for_part(obj):
    # bounds_center は Blender の Z-up 座標を返す。部品の分類はモデルを作った
    # OBJ の座標系で行い、前後・左右を取り違えない。
    center = source_point(bounds_center(obj))
    # EnemyR の脚は左右で Z が約 +/-0.75、前後で X が分かれている。付け根への
    # 距離だけで選ぶと、ひざ下の装甲を胴体や反対側の脚へ誤割当してしまうため、
    # モデル上の実際の四脚領域を先に判定する。
    is_leg_part = (
        abs(center.z) >= 0.55
        and -1.25 <= center.x <= 1.75
        and center.y <= 1.62
    )
    if is_leg_part:
        is_front = center.x >= 0.25
        is_right = center.z >= 0.0
        if is_front:
            leg_name = "FrontRight" if is_right else "FrontLeft"
        else:
            leg_name = "RearRight" if is_right else "RearLeft"

        # 剛体メカ用なので、部品単位で単一ボーンへ100%固定する。縦方向で上脚・
        # ひざ下・足首・足先を分けることで、曲げても装甲が伸びず接続が保たれる。
        if center.y <= 0.08:
            return f"{leg_name}_Toe"
        if center.y <= 0.25:
            return f"{leg_name}_Ankle"
        if center.y <= 0.66:
            return f"{leg_name}_Knee"
        return f"{leg_name}_Hip"

    # 砲塔は最上段 (Y > 2.05) のみ。これより低い頭部・胴体装甲まで砲塔へ
    # 入れると、照準回転時に胴体が割れて見えてしまうため、必ず Body に残す。
    if center.y > 2.05:
        return "TurretMuzzle" if center.x > -0.20 else "Turret"
    return "Body"


def bind_rigid_part(obj, armature, bone_name):
    obj.parent = armature
    obj.parent_type = "OBJECT"
    modifier = obj.modifiers.new(name="EnemyRigArmature", type="ARMATURE")
    modifier.object = armature
    group = obj.vertex_groups.new(name=bone_name)
    group.add(list(range(len(obj.data.vertices))), 1.0, "REPLACE")


def add_run_animation(armature):
    """Create a looping four-beat mechanical run directly in the .blend file."""
    scene = bpy.context.scene
    scene.render.fps = 24
    scene.frame_start = 1
    scene.frame_end = 48

    armature.animation_data_create()
    action = bpy.data.actions.new("EnemyR_Run")
    armature.animation_data.action = action

    # ラゴゥのように後脚が胴体を押し、その後に前脚が接地する4拍ギャロップ。
    phases = {
        "FrontRight": 3.14159265,
        "FrontLeft": 4.20973416,
        "RearLeft": 0.0,
        "RearRight": 1.06814150,
    }
    keyframes = range(1, 50, 6)

    def set_rotation(pose_bone, angle, frame):
        # モデルの左右方向 (Blender のグローバル Y) を軸に曲げる。骨ごとの
        # ロール差はローカル空間へ変換して吸収するので、4脚が同じ面で曲がる。
        local_axis = pose_bone.bone.matrix_local.to_3x3().inverted() @ Vector((0.0, 1.0, 0.0))
        pose_bone.rotation_mode = "QUATERNION"
        pose_bone.rotation_quaternion = Quaternion(local_axis.normalized(), angle)
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)

    for frame in keyframes:
        cycle = (frame - 1) / 48.0 * 6.28318531
        for leg_name, phase in phases.items():
            stride = __import__("math").sin(cycle + phase)
            swing = max(0.0, stride)
            drive = max(0.0, -stride)
            rear_power = 1.15 if leg_name.startswith("Rear") else 0.88
            hip = (swing * 0.43 - drive * 0.31) * rear_power
            knee = -swing * 0.72 + drive * 0.12
            ankle = swing * 0.24 - drive * 0.18
            toe = -swing * 0.10 + drive * 0.08
            set_rotation(armature.pose.bones[f"{leg_name}_Hip"], hip, frame)
            set_rotation(armature.pose.bones[f"{leg_name}_Knee"], knee, frame)
            set_rotation(armature.pose.bones[f"{leg_name}_Ankle"], ankle, frame)
            set_rotation(armature.pose.bones[f"{leg_name}_Toe"], toe, frame)

    # タイムライン外でも繰り返し再生できるよう、全チャンネルをループにする。
    for curve in action.fcurves:
        for key in curve.keyframe_points:
            key.interpolation = "BEZIER"
        curve.modifiers.new(type="CYCLES")
    scene.frame_set(scene.frame_start)


def main():
    if not SOURCE_OBJ.exists():
        raise RuntimeError(f"Source OBJ not found: {SOURCE_OBJ}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    clear_scene()
    bpy.ops.wm.obj_import(filepath=str(SOURCE_OBJ))
    imported = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    if not imported:
        raise RuntimeError("EnemyR OBJ import produced no meshes")

    armature = create_armature()
    for obj in imported:
        bone_name = choose_bone_for_part(obj)
        bind_rigid_part(obj, armature, bone_name)
    add_run_animation(armature)

    # Keep the original material appearance and export a portable skinned model.
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND))
    bpy.ops.export_scene.gltf(
        filepath=str(OUTPUT_GLTF),
        export_format="GLTF_SEPARATE",
        export_animations=True,
        export_yup=True,
    )
    print(f"RIGGED_ENEMY_OUTPUT={OUTPUT_GLTF}")


if __name__ == "__main__":
    main()
