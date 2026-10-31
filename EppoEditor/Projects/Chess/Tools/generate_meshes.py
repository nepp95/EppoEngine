"""Generates every mesh the Chess project uses as binary glTF (.glb).

The engine has no per-entity color, so each color variant is its own mesh with
its own glTF material. Run from anywhere with Python 3 (standard library only):

    python EppoEditor/Projects/Chess/Tools/generate_meshes.py

Outputs:
    Assets/Meshes/*.glb                    the meshes
    Assets/AssetRegistry.json              mesh entries (other entries are kept)
    Scripts/Source/Generated/MeshHandles.cs  asset handles for the scripts

Handles are a hash of the mesh name, so regenerating never changes them.

Conventions: 1 unit = one board square, +Y up, the board's top surface is y=0,
and pieces face -Z (towards Black when seen from White's side).
"""

import json
import math
import struct
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
MESH_DIR = PROJECT_DIR / "Assets" / "Meshes"
REGISTRY_PATH = PROJECT_DIR / "Assets" / "AssetRegistry.json"
HANDLES_PATH = PROJECT_DIR / "Scripts" / "Source" / "Generated" / "MeshHandles.cs"

SEGMENTS = 40


# --------------------------------------------------------------------------------------
# Geometry builder
# --------------------------------------------------------------------------------------

def normalize(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if length < 1e-9:
        return (0.0, 1.0, 0.0)
    return (v[0] / length, v[1] / length, v[2] / length)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def tangent_for(normal):
    # Any unit vector perpendicular to the normal; meshes carry no normal maps, but
    # shipping tangents keeps the engine from deriving them from placeholder UVs.
    axis = (0.0, 1.0, 0.0) if abs(normal[1]) < 0.9 else (1.0, 0.0, 0.0)
    t = normalize(cross(axis, normal))
    return (t[0], t[1], t[2], 1.0)


class MeshBuilder:
    def __init__(self):
        self.positions = []
        self.normals = []
        self.uvs = []
        self.indices = []

    def vertex(self, p, n, uv=(0.0, 0.0)):
        self.positions.append(p)
        self.normals.append(normalize(n))
        self.uvs.append(uv)
        return len(self.positions) - 1

    def triangle(self, a, b, c):
        self.indices.extend((a, b, c))

    def quad(self, a, b, c, d):
        # Counter-clockwise a-b-c-d seen from the front.
        self.indices.extend((a, b, c, a, c, d))

    def flat_quad(self, p0, p1, p2, p3):
        n = normalize(cross(sub(p1, p0), sub(p2, p0)))
        i = [self.vertex(p, n, uv) for p, uv in zip((p0, p1, p2, p3), ((0, 0), (1, 0), (1, 1), (0, 1)))]
        self.quad(*i)

    def box(self, center, size, yaw=0.0):
        cx, cy, cz = center
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        c, s = math.cos(yaw), math.sin(yaw)

        def p(x, y, z):
            return (cx + x * c + z * s, cy + y, cz - x * s + z * c)

        corners = {
            (sx, sy, sz): p(sx * hx, sy * hy, sz * hz)
            for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)
        }
        faces = [
            ((1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)),        # +X
            ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)),    # -X
            ((-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)),        # +Y
            ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),    # -Y
            ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)),        # +Z
            ((1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)),    # -Z
        ]
        for face in faces:
            self.flat_quad(*(corners[k] for k in face))

    def lathe(self, profile, offset=(0.0, 0.0, 0.0), crease_degrees=40.0, segments=SEGMENTS):
        """Revolves a (radius, height) profile, listed bottom to top, around +Y.

        Neighbouring profile segments share normals unless they bend more than
        crease_degrees, which keeps rims crisp while curves stay smooth."""
        ox, oy, oz = offset
        seg_normals = []
        for (r0, y0), (r1, y1) in zip(profile, profile[1:]):
            dr, dy = r1 - r0, y1 - y0
            # Outward normal of the segment in the (r, y) plane.
            length = math.hypot(dr, dy) or 1.0
            seg_normals.append((dy / length, -dr / length))

        cos_crease = math.cos(math.radians(crease_degrees))

        def blended(a, b):
            if a[0] * b[0] + a[1] * b[1] >= cos_crease:
                n = (a[0] + b[0], a[1] + b[1])
                length = math.hypot(*n) or 1.0
                return (n[0] / length, n[1] / length)
            return None

        total = sum(math.hypot(r1 - r0, y1 - y0) for (r0, y0), (r1, y1) in zip(profile, profile[1:])) or 1.0
        travelled = 0.0

        for s, ((r0, y0), (r1, y1)) in enumerate(zip(profile, profile[1:])):
            if abs(r0) < 1e-9 and abs(r1) < 1e-9:
                continue

            n_start = seg_normals[s]
            n_end = seg_normals[s]
            if s > 0:
                n_start = blended(seg_normals[s - 1], seg_normals[s]) or seg_normals[s]
            if s + 1 < len(seg_normals):
                n_end = blended(seg_normals[s], seg_normals[s + 1]) or seg_normals[s]

            v0 = travelled / total
            travelled += math.hypot(r1 - r0, y1 - y0)
            v1 = travelled / total

            ring0, ring1 = [], []
            for i in range(segments + 1):
                a = 2.0 * math.pi * i / segments
                ca, sa = math.cos(a), math.sin(a)
                u = i / segments
                ring0.append(self.vertex((ox + r0 * ca, oy + y0, oz + r0 * sa), (n_start[0] * ca, n_start[1], n_start[0] * sa), (u, v0)))
                ring1.append(self.vertex((ox + r1 * ca, oy + y1, oz + r1 * sa), (n_end[0] * ca, n_end[1], n_end[0] * sa), (u, v1)))

            for i in range(segments):
                # Seen from outside, increasing angle runs clockwise around +Y, so
                # going up first keeps the winding counter-clockwise. A ring of
                # radius zero (a pole) collapses its half of the quad; skip it.
                a, b, c, d = ring0[i], ring1[i], ring1[i + 1], ring0[i + 1]
                if abs(r0) > 1e-9:
                    self.triangle(a, c, d)
                if abs(r1) > 1e-9:
                    self.triangle(a, b, c)

    def sphere(self, center, radius, steps=12):
        profile = []
        for i in range(steps + 1):
            a = -math.pi / 2 + math.pi * i / steps
            profile.append((radius * math.cos(a), radius * math.sin(a)))
        profile[0] = (0.0, -radius)
        profile[-1] = (0.0, radius)
        self.lathe(profile, offset=center, crease_degrees=90.0, segments=max(8, steps * 2))

    def extrude(self, polygon, half_thickness):
        """Extrudes a simple (z, y) polygon along X by +/-half_thickness."""
        if signed_area(polygon) < 0:
            polygon = list(reversed(polygon))
        triangles = ear_clip(polygon)

        for side in (1, -1):
            x = side * half_thickness
            ids = [self.vertex((x, y, z), (side, 0.0, 0.0), (z, y)) for z, y in polygon]
            for a, b, c in triangles:
                # The polygon is CCW in (z, y); viewed from +X that maps to CW.
                if side > 0:
                    self.triangle(ids[a], ids[c], ids[b])
                else:
                    self.triangle(ids[a], ids[b], ids[c])

        count = len(polygon)
        for i in range(count):
            z0, y0 = polygon[i]
            z1, y1 = polygon[(i + 1) % count]
            self.flat_quad(
                (half_thickness, y0, z0),
                (-half_thickness, y0, z0),
                (-half_thickness, y1, z1),
                (half_thickness, y1, z1),
            )

    def flip_z(self):
        self.positions = [(x, y, -z) for x, y, z in self.positions]
        self.normals = [(x, y, -z) for x, y, z in self.normals]
        # Mirroring inverts winding.
        for i in range(0, len(self.indices), 3):
            self.indices[i + 1], self.indices[i + 2] = self.indices[i + 2], self.indices[i + 1]


def signed_area(polygon):
    return 0.5 * sum(
        polygon[i][0] * polygon[(i + 1) % len(polygon)][1] - polygon[(i + 1) % len(polygon)][0] * polygon[i][1]
        for i in range(len(polygon))
    )


def ear_clip(polygon):
    """Triangulates a counter-clockwise simple polygon; returns index triples."""

    def is_convex(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) > 1e-12

    def inside(p, a, b, c):
        d1 = (p[0] - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (p[1] - b[1])
        d2 = (p[0] - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (p[1] - c[1])
        d3 = (p[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (p[1] - a[1])
        has_neg = d1 < 0 or d2 < 0 or d3 < 0
        has_pos = d1 > 0 or d2 > 0 or d3 > 0
        return not (has_neg and has_pos)

    remaining = list(range(len(polygon)))
    triangles = []
    guard = 0
    while len(remaining) > 3 and guard < 10000:
        guard += 1
        for k in range(len(remaining)):
            i0, i1, i2 = remaining[k - 1], remaining[k], remaining[(k + 1) % len(remaining)]
            a, b, c = polygon[i0], polygon[i1], polygon[i2]
            if not is_convex(a, b, c):
                continue
            if any(inside(polygon[j], a, b, c) for j in remaining if j not in (i0, i1, i2)):
                continue
            triangles.append((i0, i1, i2))
            del remaining[k]
            break
        else:
            raise RuntimeError("Polygon could not be triangulated (self-intersecting?)")
    triangles.append(tuple(remaining))
    return triangles


# --------------------------------------------------------------------------------------
# Pieces
# --------------------------------------------------------------------------------------

BASE = [(0.0, 0.0), (0.35, 0.0), (0.35, 0.05), (0.32, 0.075), (0.32, 0.105), (0.27, 0.13)]


def arc(cx, cy, radius, start_deg, end_deg, steps):
    return [
        (cx + radius * math.cos(math.radians(start_deg + (end_deg - start_deg) * i / steps)),
         cy + radius * math.sin(math.radians(start_deg + (end_deg - start_deg) * i / steps)))
        for i in range(steps + 1)
    ]


def pawn():
    m = MeshBuilder()
    head = arc(0.0, 0.47, 0.115, -55, 90, 10)
    m.lathe(BASE + [(0.21, 0.15), (0.13, 0.22), (0.095, 0.32), (0.15, 0.345), (0.15, 0.365), (0.075, 0.38)]
            + [(max(r, 0.0), y) for r, y in head[:-1]] + [(0.0, 0.585)])
    return m


def rook():
    m = MeshBuilder()
    m.lathe(BASE + [(0.24, 0.15), (0.20, 0.22), (0.185, 0.47), (0.24, 0.51), (0.25, 0.60),
                    (0.17, 0.60), (0.17, 0.56), (0.0, 0.56)])
    for i in range(4):
        yaw = math.pi / 4 + i * math.pi / 2
        r = 0.21
        m.box((r * math.cos(yaw), 0.635, r * math.sin(yaw)), (0.09, 0.07, 0.13), yaw=-yaw)
    return m


def knight():
    m = MeshBuilder()
    m.lathe(BASE + [(0.24, 0.15), (0.19, 0.21), (0.18, 0.27), (0.0, 0.27)])
    # Side profile of the head, (forward, up); the nose points +forward.
    head = [
        (-0.19, 0.24), (0.15, 0.24), (0.12, 0.31), (0.04, 0.39), (0.19, 0.49),
        (0.25, 0.56), (0.22, 0.63), (0.12, 0.68), (0.03, 0.73), (0.00, 0.81),
        (-0.06, 0.74), (-0.14, 0.68), (-0.21, 0.56), (-0.23, 0.40),
    ]
    head_mesh = MeshBuilder()
    head_mesh.extrude(head, 0.105)
    # Eyes: small boxes on both sides near the brow.
    head_mesh.box((0.0, 0.63, 0.10), (0.23, 0.035, 0.05))
    head_mesh.flip_z()  # Face -Z like every other piece.
    merge(m, head_mesh)
    return m


def bishop():
    m = MeshBuilder()
    m.lathe(BASE + [(0.22, 0.15), (0.13, 0.24), (0.09, 0.42), (0.16, 0.45), (0.16, 0.47), (0.08, 0.49),
                    (0.12, 0.54), (0.145, 0.60), (0.13, 0.665), (0.08, 0.725), (0.025, 0.76), (0.0, 0.76)])
    m.sphere((0.0, 0.79, 0.0), 0.04)
    return m


def queen():
    m = MeshBuilder()
    m.lathe(BASE + [(0.25, 0.15), (0.145, 0.26), (0.10, 0.52), (0.18, 0.555), (0.18, 0.575), (0.10, 0.60),
                    (0.13, 0.66), (0.20, 0.78), (0.185, 0.80), (0.09, 0.79), (0.08, 0.84), (0.04, 0.87), (0.0, 0.875)])
    m.sphere((0.0, 0.905, 0.0), 0.045)
    for i in range(8):
        a = 2.0 * math.pi * i / 8
        m.sphere((0.19 * math.cos(a), 0.81, 0.19 * math.sin(a)), 0.03, steps=8)
    return m


def king():
    m = MeshBuilder()
    m.lathe(BASE + [(0.26, 0.15), (0.155, 0.26), (0.11, 0.58), (0.195, 0.615), (0.195, 0.635), (0.11, 0.66),
                    (0.15, 0.74), (0.175, 0.80), (0.10, 0.83), (0.0, 0.84)])
    m.box((0.0, 0.93, 0.0), (0.055, 0.20, 0.055))
    m.box((0.0, 0.95, 0.0), (0.17, 0.055, 0.055))
    return m


def merge(dst, src):
    base = len(dst.positions)
    dst.positions += src.positions
    dst.normals += src.normals
    dst.uvs += src.uvs
    dst.indices += [i + base for i in src.indices]


# --------------------------------------------------------------------------------------
# Board and markers
# --------------------------------------------------------------------------------------

def tile():
    m = MeshBuilder()
    m.box((0.0, -0.05, 0.0), (1.0, 0.1, 1.0))
    return m


def frame():
    m = MeshBuilder()
    # Wooden border slab the tiles sit in, plus a darker plinth below it.
    m.box((0.0, -0.13, 0.0), (9.0, 0.24, 9.0))
    m.box((0.0, -0.33, 0.0), (9.4, 0.18, 9.4))
    return m


def table():
    m = MeshBuilder()
    m.box((0.0, -0.52, 0.0), (40.0, 0.2, 40.0))
    return m


def overlay(height):
    m = MeshBuilder()
    m.box((0.0, height / 2 + 0.002, 0.0), (0.98, height, 0.98))
    return m


def outline(height, inner=0.80):
    m = MeshBuilder()
    w = (0.98 - inner) / 2
    y = height / 2 + 0.002
    off = 0.49 - w / 2
    m.box((0.0, y, off), (0.98, height, w))
    m.box((0.0, y, -off), (0.98, height, w))
    m.box((off, y, 0.0), (w, height, inner))
    m.box((-off, y, 0.0), (w, height, inner))
    return m


def dot():
    m = MeshBuilder()
    m.lathe([(0.0, 0.0), (0.15, 0.0), (0.15, 0.03), (0.0, 0.03)])
    return m


# --------------------------------------------------------------------------------------
# glTF writer
# --------------------------------------------------------------------------------------

def material(name, color, roughness, metallic=0.0, emissive=(0.0, 0.0, 0.0), strength=1.0):
    mat = {
        "name": name,
        "pbrMetallicRoughness": {
            "baseColorFactor": [color[0], color[1], color[2], 1.0],
            "metallicFactor": metallic,
            "roughnessFactor": roughness,
        },
        "emissiveFactor": list(emissive),
    }
    if strength != 1.0:
        mat["extensions"] = {"KHR_materials_emissive_strength": {"emissiveStrength": strength}}
    return mat


def write_glb(path, mesh, mat):
    positions = b"".join(struct.pack("<3f", *p) for p in mesh.positions)
    normals = b"".join(struct.pack("<3f", *n) for n in mesh.normals)
    uvs = b"".join(struct.pack("<2f", *uv) for uv in mesh.uvs)
    tangents = b"".join(struct.pack("<4f", *tangent_for(n)) for n in mesh.normals)
    indices = struct.pack("<%dI" % len(mesh.indices), *mesh.indices)

    blobs = [positions, normals, uvs, tangents, indices]
    views, offset, binary = [], 0, b""
    for i, blob in enumerate(blobs):
        views.append({
            "buffer": 0,
            "byteOffset": offset,
            "byteLength": len(blob),
            "target": 34963 if i == len(blobs) - 1 else 34962,
        })
        binary += blob
        offset += len(blob)

    count = len(mesh.positions)
    mins = [min(p[i] for p in mesh.positions) for i in range(3)]
    maxs = [max(p[i] for p in mesh.positions) for i in range(3)]
    doc = {
        "asset": {"version": "2.0", "generator": "EppoEngine Chess generate_meshes.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": path.stem}],
        "meshes": [{
            "name": path.stem,
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2, "TANGENT": 3},
                "indices": 4,
                "material": 0,
            }],
        }],
        "materials": [mat],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3", "min": mins, "max": maxs},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5126, "count": count, "type": "VEC4"},
            {"bufferView": 4, "componentType": 5125, "count": len(mesh.indices), "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
    }
    if any("extensions" in m for m in doc["materials"]):
        doc["extensionsUsed"] = ["KHR_materials_emissive_strength"]

    json_chunk = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    json_chunk += b" " * (-len(json_chunk) % 4)
    binary += b"\0" * (-len(binary) % 4)

    total = 12 + 8 + len(json_chunk) + 8 + len(binary)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(json_chunk), b"JSON"))
        f.write(json_chunk)
        f.write(struct.pack("<I4s", len(binary), b"BIN\0"))
        f.write(binary)


def handle_for(name):
    # FNV-1a 64; stable across runs. Handles 1-9 are reserved for built-in primitives.
    h = 0xCBF29CE484222325
    for byte in ("Chess.Mesh." + name).encode("utf-8"):
        h ^= byte
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    assert h > 9
    return h


# --------------------------------------------------------------------------------------
# Catalogue
# --------------------------------------------------------------------------------------

WHITE = dict(color=(0.86, 0.80, 0.68), roughness=0.38)
BLACK = dict(color=(0.035, 0.035, 0.04), roughness=0.30)

PIECES = {"Pawn": pawn, "Knight": knight, "Bishop": bishop, "Rook": rook, "Queen": queen, "King": king}


def catalogue():
    meshes = {}
    for piece, build in PIECES.items():
        meshes["White" + piece] = (build, material("White", **WHITE))
        meshes["Black" + piece] = (build, material("Black", **BLACK))

    meshes["SquareLight"] = (tile, material("SquareLight", (0.80, 0.66, 0.45), 0.55))
    meshes["SquareDark"] = (tile, material("SquareDark", (0.27, 0.15, 0.07), 0.55))
    meshes["BoardFrame"] = (frame, material("BoardFrame", (0.09, 0.045, 0.02), 0.45))
    meshes["Table"] = (table, material("Table", (0.10, 0.11, 0.12), 0.8))

    meshes["MarkerLastMove"] = (lambda: overlay(0.004), material("MarkerLastMove", (0.75, 0.62, 0.15), 0.6, emissive=(0.25, 0.20, 0.03)))
    meshes["MarkerSelected"] = (lambda: overlay(0.008), material("MarkerSelected", (0.25, 0.65, 0.25), 0.6, emissive=(0.10, 0.45, 0.10)))
    meshes["MarkerCheck"] = (lambda: overlay(0.006), material("MarkerCheck", (0.80, 0.10, 0.08), 0.6, emissive=(0.90, 0.08, 0.05)))
    meshes["MarkerHover"] = (lambda: outline(0.014, inner=0.86), material("MarkerHover", (0.95, 0.90, 0.55), 0.5, emissive=(0.90, 0.80, 0.30)))
    meshes["MarkerCapture"] = (lambda: outline(0.012), material("MarkerCapture", (0.85, 0.20, 0.12), 0.5, emissive=(0.70, 0.12, 0.06)))
    meshes["MarkerMove"] = (dot, material("MarkerMove", (0.25, 0.55, 0.85), 0.5, emissive=(0.12, 0.35, 0.70)))
    return meshes


def main():
    MESH_DIR.mkdir(parents=True, exist_ok=True)
    meshes = catalogue()

    for name, (build, mat) in meshes.items():
        write_glb(MESH_DIR / (name + ".glb"), build(), mat)

    registry = json.loads(REGISTRY_PATH.read_text()) if REGISTRY_PATH.exists() else {"Assets": []}
    generated = {"Meshes\\" + name + ".glb" for name in meshes}
    assets = [a for a in registry["Assets"] if a["Filepath"] not in generated]
    assets += [{"Filepath": "Meshes\\" + name + ".glb", "Handle": handle_for(name), "Type": "Mesh"} for name in meshes]
    assets.sort(key=lambda a: a["Filepath"])
    REGISTRY_PATH.write_text(json.dumps({"Assets": assets}, indent=4) + "\n")

    HANDLES_PATH.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "// <auto-generated> by Tools/generate_meshes.py; do not edit by hand. </auto-generated>",
        "namespace Chess",
        "{",
        "    public static class MeshHandles",
        "    {",
    ]
    lines += ["        public const ulong %s = %dUL;" % (name, handle_for(name)) for name in meshes]
    lines += ["    }", "}", ""]
    HANDLES_PATH.write_text("\n".join(lines))

    print("Wrote %d meshes to %s" % (len(meshes), MESH_DIR))


if __name__ == "__main__":
    main()
