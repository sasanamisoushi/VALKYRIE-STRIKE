import math
import os

os.makedirs("project/resources/models", exist_ok=True)

def generate_inward_cylinder_obj(filename, mtl_filename, mat_name, radius, z_min, z_max, radial_segs, length_segs, u_tile, v_tile):
    verts = []
    uvs = []
    normals = []
    faces = []

    # 頂点生成
    # 円周方向 theta: 0 から 2*pi
    # 長さ方向 z: z_min から z_max
    for j in range(length_segs + 1):
        v_coord = (j / length_segs) * v_tile
        z = z_min + (z_max - z_min) * (j / length_segs)
        for i in range(radial_segs + 1):
            u_coord = (i / radial_segs) * u_tile
            theta = (i / radial_segs) * 2.0 * math.pi
            
            x = math.cos(theta) * radius
            y = math.sin(theta) * radius
            
            verts.append((x, y, z))
            uvs.append((u_coord, v_coord))
            # 内側法線: (0,0,z) へ向かうベクトル
            normals.append((-math.cos(theta), -math.sin(theta), 0.0))

    # 面生成 (内側から見て反時計回りになるインデックス順)
    # i, j の格子
    stride = radial_segs + 1
    for j in range(length_segs):
        for i in range(radial_segs):
            idx0 = j * stride + i + 1         # 左下 (1-indexed)
            idx1 = j * stride + (i + 1) + 1   # 右下
            idx2 = (j + 1) * stride + i + 1   # 左上
            idx3 = (j + 1) * stride + (i + 1) + 1 # 右上

            # 内側から見たとき:
            # 三角形1: idx0 -> idx2 -> idx1
            # 三角形2: idx1 -> idx2 -> idx3
            faces.append((idx0, idx2, idx1))
            faces.append((idx1, idx2, idx3))

    obj_path = os.path.join("project/resources/models", filename)
    with open(obj_path, "w", encoding="utf-8") as f:
        f.write(f"mtllib {mtl_filename}\n")
        f.write(f"usemtl {mat_name}\n")
        for v in verts:
            f.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
        for vt in uvs:
            f.write(f"vt {vt[0]:.4f} {vt[1]:.4f}\n")
        for vn in normals:
            f.write(f"vn {vn[0]:.4f} {vn[1]:.4f} {vn[2]:.4f}\n")
        for tri in faces:
            f.write(f"f {tri[0]}/{tri[0]}/{tri[0]} {tri[1]}/{tri[1]}/{tri[1]} {tri[2]}/{tri[2]}/{tri[2]}\n")

    print(f"Generated {obj_path} with {len(verts)} vertices and {len(faces)} triangles.")

# 1. 外側シリンダー (warp_tunnel.obj)
generate_inward_cylinder_obj(
    filename="warp_tunnel.obj",
    mtl_filename="warp_tunnel.mtl",
    mat_name="WarpTunnelMat",
    radius=18.0,
    z_min=-60.0,
    z_max=260.0,
    radial_segs=64,
    length_segs=32,
    u_tile=2.0,
    v_tile=10.0
)

# mtl
with open("project/resources/models/warp_tunnel.mtl", "w", encoding="utf-8") as f:
    f.write("newmtl WarpTunnelMat\n")
    f.write("Ka 1.0 1.0 1.0\n")
    f.write("Kd 1.0 1.0 1.0\n")
    f.write("Ks 0.0 0.0 0.0\n")
    f.write("d 1.0\n")
    f.write("illum 1\n")
    f.write("map_Kd warp_nebula_stars.png\n")

# 2. 内側シリンダー (warp_tunnel_inner.obj)
generate_inward_cylinder_obj(
    filename="warp_tunnel_inner.obj",
    mtl_filename="warp_tunnel_inner.mtl",
    mat_name="WarpInnerMat",
    radius=15.0,
    z_min=-60.0,
    z_max=260.0,
    radial_segs=48,
    length_segs=24,
    u_tile=3.0,
    v_tile=8.0
)

# mtl
with open("project/resources/models/warp_tunnel_inner.mtl", "w", encoding="utf-8") as f:
    f.write("newmtl WarpInnerMat\n")
    f.write("Ka 1.0 1.0 1.0\n")
    f.write("Kd 1.0 1.0 1.0\n")
    f.write("Ks 0.0 0.0 0.0\n")
    f.write("d 0.8\n")
    f.write("illum 1\n")
    f.write("map_Kd warp_stream_lines.png\n")

print("Generated mtl files.")
