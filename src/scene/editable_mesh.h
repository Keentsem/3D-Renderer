#ifndef EDITABLE_MESH_H
#define EDITABLE_MESH_H

#include <array>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include "../core/vec.h"

// ============================================================
// EditableMesh — small triangle soup that the UI can mutate
// ============================================================
// Indexed triangle list. Extrude grows the mesh by duplicating a
// face's verts along its normal and stitching side quads. Subdivide
// splits each triangle into four via edge midpoints (naive, fine for
// a demo — creates a few duplicate verts but flat shading doesn't
// care).
// ============================================================
struct EditableMesh {
    using Tri = std::array<int, 3>;

    std::vector<Vec3> verts;
    std::vector<Tri>  faces;

    // -------- Primitives ----------------------------------------------------
    static EditableMesh make_cube() {
        EditableMesh m;
        m.verts = {
            {-0.5,-0.5,-0.5},{ 0.5,-0.5,-0.5},
            { 0.5, 0.5,-0.5},{-0.5, 0.5,-0.5},
            {-0.5,-0.5, 0.5},{ 0.5,-0.5, 0.5},
            { 0.5, 0.5, 0.5},{-0.5, 0.5, 0.5},
        };
        m.faces = {
            {4,5,6},{4,6,7},   // +Z front
            {1,0,3},{1,3,2},   // -Z back
            {5,1,2},{5,2,6},   // +X right
            {0,4,7},{0,7,3},   // -X left
            {7,6,2},{7,2,3},   // +Y top
            {0,1,5},{0,5,4},   // -Y bottom
        };
        return m;
    }

    static EditableMesh make_octahedron() {
        EditableMesh m;
        m.verts = {
            { 0.0, 0.75, 0.0},   // 0 top
            { 0.0,-0.75, 0.0},   // 1 bot
            { 0.65, 0.0, 0.0},   // 2 +X
            {-0.65, 0.0, 0.0},   // 3 -X
            { 0.0, 0.0, 0.65},   // 4 +Z
            { 0.0, 0.0,-0.65},   // 5 -Z
        };
        m.faces = {
            {0,2,4},{0,4,3},{0,3,5},{0,5,2},
            {1,4,2},{1,3,4},{1,5,3},{1,2,5},
        };
        return m;
    }

    static EditableMesh make_tetrahedron() {
        EditableMesh m;
        const double s = 0.72;
        m.verts = {
            { s, s, s},{ s,-s,-s},{-s, s,-s},{-s,-s, s},
        };
        m.faces = { {0,1,2},{0,3,1},{0,2,3},{1,3,2} };
        return m;
    }

    // Ghost mascot — loads from assets/ghost.obj (428 verts, 448 faces).
    // Falls back to procedural generation if file not found.
    static EditableMesh make_mascot() {
        // Try loading the high-quality OBJ model first
        const char* paths[] = {
            "assets/ghost.obj",
            "../assets/ghost.obj",
            "../../assets/ghost.obj",
        };
        for (const char* p : paths) {
            EditableMesh loaded = load_obj(p);
            if (!loaded.verts.empty()) return loaded;
        }

        // Fallback: procedural ghost
        EditableMesh m;
        const int S = 24;  // segments per ring — very smooth

        // Profile: (height, radius) pairs defining the ghost cross-section.
        // Classic ghost = perfect hemisphere on top, straight cylinder body,
        // then a flared skirt that breaks into zigzag tentacles.
        struct Ring { double y, r; };
        const Ring profile[] = {
            // --- Dome (hemisphere, r=0.55, center at y=0.15) ---
            {0.70, 0.00},  // 0  apex (degenerate ring → fan)
            {0.67, 0.15},  // 1
            {0.60, 0.30},  // 2
            {0.50, 0.42},  // 3
            {0.38, 0.51},  // 4
            {0.25, 0.55},  // 5  widest = equator
            // --- Body (straight cylinder, slight taper) ---
            {0.10, 0.55},  // 6
            {-0.05, 0.54}, // 7
            {-0.20, 0.53}, // 8
            {-0.35, 0.52}, // 9
            {-0.48, 0.52}, // 10  bottom of body
        };
        const int N_RINGS = 11;

        // Vertex 0 = apex point
        m.verts.push_back({0.0, profile[0].y, 0.0});

        // Rings 1..N_RINGS-1 → S verts each
        // Ring r starts at vertex index: 1 + (r-1)*S
        for (int r = 1; r < N_RINGS; r++) {
            for (int i = 0; i < S; i++) {
                double a = i * 2.0 * M_PI / S;
                double x = profile[r].r * std::cos(a);
                double z = profile[r].r * std::sin(a);
                m.verts.push_back({x, profile[r].y, z});
            }
        }

        // --- Tentacle skirt: 4 tentacles, zigzag bottom ---
        // Two rings: flare ring + zigzag tip ring
        int skirt_base = (int)m.verts.size();
        // Flare ring — slightly wider, at y = -0.55
        for (int i = 0; i < S; i++) {
            double a = i * 2.0 * M_PI / S;
            m.verts.push_back({0.54 * std::cos(a), -0.55, 0.54 * std::sin(a)});
        }
        // Zigzag tip ring — alternating high/low creating 4 tentacle lobes
        int tip_base = (int)m.verts.size();
        for (int i = 0; i < S; i++) {
            double a = i * 2.0 * M_PI / S;
            // 4 lobes: cos(4*a) gives peaks at 0°, 90°, 180°, 270°
            double lobe = std::cos(i * 4.0 * M_PI / S);
            // Peaks (lobe=+1) → tentacle tip hangs low
            // Valleys (lobe=-1) → scallop goes back up
            double tip_y = -0.68 + lobe * (-0.18);  // tips at -0.86, scallops at -0.50
            double tip_r = 0.42 + lobe * 0.10;       // wider at tips
            m.verts.push_back({tip_r * std::cos(a), tip_y, tip_r * std::sin(a)});
        }

        // --- Eyes: two spherical bumps on the front face ---
        // Each eye = 8-vert ring + center, pushed OUT slightly for 3D bump
        auto add_eye = [&](double cx, double cy, double cz, double er) {
            int center = (int)m.verts.size();
            // Center pushed outward (bulge)
            double face_nx = cx / std::sqrt(cx*cx + cz*cz + 0.001);
            double face_nz = cz / std::sqrt(cx*cx + cz*cz + 0.001);
            m.verts.push_back({cx + face_nx * 0.04, cy, cz + face_nz * 0.04});
            // Ring around eye — sits on the body surface
            for (int i = 0; i < 8; i++) {
                double a = i * 2.0 * M_PI / 8.0;
                // Eye ring in the tangent plane of the face
                double ex = cx + er * std::cos(a);
                double ey = cy + er * std::sin(a) * 1.2;  // slightly taller than wide
                m.verts.push_back({ex, ey, cz});
            }
            // Pupil — smaller, pushed even further out
            int pupil = (int)m.verts.size();
            m.verts.push_back({cx + face_nx * 0.07, cy - 0.02, cz + face_nz * 0.07});
            for (int i = 0; i < 6; i++) {
                double a = i * 2.0 * M_PI / 6.0;
                double px = cx + face_nx * 0.04 + er * 0.45 * std::cos(a);
                double py = cy - 0.02 + er * 0.45 * std::sin(a);
                m.verts.push_back({px, py, cz + face_nz * 0.04});
            }
            return std::make_pair(center, pupil);
        };

        // Eyes positioned on the front face (z+), spaced apart
        auto [eye_l, pupil_l] = add_eye(-0.18, 0.30, 0.48, 0.10);
        auto [eye_r, pupil_r] = add_eye( 0.18, 0.30, 0.48, 0.10);

        // --- Mouth: "O" shape, slightly indented ---
        int mouth_c = (int)m.verts.size();
        m.verts.push_back({0.0, 0.08, 0.51});  // pushed out slightly
        for (int i = 0; i < 8; i++) {
            double a = i * 2.0 * M_PI / 8.0;
            m.verts.push_back({0.06 * std::cos(a), 0.08 + 0.08 * std::sin(a), 0.50});
        }

        // ================ FACES ================

        // Top cap fan: apex (0) → ring 1
        int ring1 = 1;
        for (int i = 0; i < S; i++) {
            int j = (i + 1) % S;
            m.faces.push_back({0, ring1 + i, ring1 + j});
        }

        // Ring-to-ring quads for body (rings 1..N_RINGS-1)
        for (int r = 1; r < N_RINGS - 1; r++) {
            int a_base = 1 + (r - 1) * S;
            int b_base = 1 + r * S;
            for (int i = 0; i < S; i++) {
                int j = (i + 1) % S;
                m.faces.push_back({a_base + i, b_base + i, b_base + j});
                m.faces.push_back({a_base + i, b_base + j, a_base + j});
            }
        }

        // Last body ring → skirt flare ring
        int last_ring = 1 + (N_RINGS - 2) * S;
        for (int i = 0; i < S; i++) {
            int j = (i + 1) % S;
            m.faces.push_back({last_ring + i, skirt_base + i, skirt_base + j});
            m.faces.push_back({last_ring + i, skirt_base + j, last_ring + j});
        }

        // Skirt flare → zigzag tips
        for (int i = 0; i < S; i++) {
            int j = (i + 1) % S;
            m.faces.push_back({skirt_base + i, tip_base + i, tip_base + j});
            m.faces.push_back({skirt_base + i, tip_base + j, skirt_base + j});
        }

        // Eye fans (outer ring)
        auto fan8 = [&](int center) {
            for (int i = 0; i < 8; i++) {
                int j = (i + 1) % 8;
                m.faces.push_back({center, center + 1 + i, center + 1 + j});
            }
        };
        fan8(eye_l);
        fan8(eye_r);

        // Pupil fans (6-gon)
        auto fan6 = [&](int center) {
            for (int i = 0; i < 6; i++) {
                int j = (i + 1) % 6;
                m.faces.push_back({center, center + 1 + i, center + 1 + j});
            }
        };
        fan6(pupil_l);
        fan6(pupil_r);

        // Mouth fan (8-gon)
        fan8(mouth_c);

        return m;
    }

    // -------- Geometry queries ---------------------------------------------
    Vec3 face_center(int f) const {
        const auto& t = faces[f];
        return (verts[t[0]] + verts[t[1]] + verts[t[2]]) / 3.0;
    }

    Vec3 face_normal(int f) const {
        const auto& t = faces[f];
        Vec3 e1 = verts[t[1]] - verts[t[0]];
        Vec3 e2 = verts[t[2]] - verts[t[0]];
        Vec3 n  = cross(e1, e2);
        double len = n.norm();
        return len > 1e-9 ? n / len : Vec3(0, 1, 0);
    }

    int vertex_count() const { return (int)verts.size(); }
    int face_count()   const { return (int)faces.size(); }

    // Smooth per-vertex normals = area-weighted sum of adjacent face
    // normals, then normalize. Good enough for small test meshes.
    std::vector<Vec3> compute_vertex_normals() const {
        std::vector<Vec3> vn(verts.size(), Vec3(0, 0, 0));
        for (const auto& t : faces) {
            Vec3 e1 = verts[t[1]] - verts[t[0]];
            Vec3 e2 = verts[t[2]] - verts[t[0]];
            Vec3 fn = cross(e1, e2);   // length = 2 * area — gives area weighting for free
            vn[t[0]] = vn[t[0]] + fn;
            vn[t[1]] = vn[t[1]] + fn;
            vn[t[2]] = vn[t[2]] + fn;
        }
        for (auto& n : vn) {
            double len = n.norm();
            if (len > 1e-9) n = n / len;
            else            n = Vec3(0, 1, 0);
        }
        return vn;
    }

    // -------- Mutation ------------------------------------------------------
    void translate_vertex(int v, const Vec3& delta) {
        if (v >= 0 && v < (int)verts.size()) verts[v] = verts[v] + delta;
    }

    // Extrude a face along its own normal. Returns the index of the
    // now-extruded top face (same as the input index).
    int extrude_face(int f, double dist) {
        if (f < 0 || f >= (int)faces.size()) return -1;
        Vec3 n   = face_normal(f);
        Tri  old = faces[f];
        int base = (int)verts.size();
        for (int i = 0; i < 3; i++)
            verts.push_back(verts[old[i]] + n * dist);

        // Replace the top face with the pushed-up triangle.
        faces[f] = { base, base + 1, base + 2 };

        // Stitch 3 side quads (6 tris) winding outward so normals face out.
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            faces.push_back({ old[i],    old[j],    base + j });
            faces.push_back({ old[i],    base + j,  base + i });
        }
        return f;
    }

    // Split every triangle into 4 via edge midpoints. Duplicates some
    // verts across faces, which is fine for flat shading.
    void subdivide_all() {
        std::vector<Tri> out;
        out.reserve(faces.size() * 4);
        std::vector<Vec3> vcopy = verts;  // freeze indices for read side
        for (const auto& t : faces) {
            const Vec3& a = vcopy[t[0]];
            const Vec3& b = vcopy[t[1]];
            const Vec3& c = vcopy[t[2]];
            int ma = (int)verts.size(); verts.push_back((a + b) * 0.5);
            int mb = (int)verts.size(); verts.push_back((b + c) * 0.5);
            int mc = (int)verts.size(); verts.push_back((c + a) * 0.5);
            out.push_back({ t[0], ma, mc });
            out.push_back({ ma, t[1], mb });
            out.push_back({ mc, mb, t[2] });
            out.push_back({ ma, mb, mc });
        }
        faces.swap(out);
    }

    // Bevel-ish inset: pull each selected face's vertices toward its center.
    void inset_face(int f, double amount) {
        if (f < 0 || f >= (int)faces.size()) return;
        Vec3 c = face_center(f);
        auto& t = faces[f];
        for (int i = 0; i < 3; i++) {
            Vec3 v = verts[t[i]];
            verts[t[i]] = v + (c - v) * amount;
        }
    }

    // -------- OBJ file loader ------------------------------------------------
    // Loads a Wavefront .obj file (vertices + triangulated faces).
    // Supports: v, f (triangles and quads auto-triangulated).
    // Returns empty mesh on failure.
    static EditableMesh load_obj(const std::string& path) {
        EditableMesh m;
        std::ifstream file(path);
        if (!file.is_open()) return m;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") {
                double x, y, z;
                iss >> x >> y >> z;
                m.verts.push_back({x, y, z});
            } else if (prefix == "f") {
                // Parse face indices (1-based, may have v/vt/vn format)
                std::vector<int> indices;
                std::string token;
                while (iss >> token) {
                    // Extract vertex index before first '/'
                    int idx = std::stoi(token.substr(0, token.find('/')));
                    indices.push_back(idx - 1);  // OBJ is 1-based
                }
                // Triangulate: fan from first vertex
                for (size_t i = 2; i < indices.size(); i++) {
                    m.faces.push_back({indices[0], indices[i-1], indices[i]});
                }
            }
        }

        // Center and normalize to unit scale
        if (!m.verts.empty()) {
            Vec3 mn = m.verts[0], mx = m.verts[0];
            for (const auto& v : m.verts) {
                mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
                mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
            }
            Vec3 center = (mn + mx) * 0.5;
            double span = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z});
            double scale = (span > 1e-9) ? 1.6 / span : 1.0;
            for (auto& v : m.verts) {
                v = (v - center) * scale;
                v.y = -v.y;  // flip Y — OBJ is Y-up but our viewport is Y-down
            }
        }
        return m;
    }
};

#endif
