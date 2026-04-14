#ifndef MODEL_H
#define MODEL_H

#include "../core/vec.h"
#include "../image/tgaimage.h"
#include <vector>
#include <string>
#include <cstdio>
#include <sstream>
#include <iostream>

// ============================================================
// Model Class - Loads and manages 3D models from OBJ files
// ============================================================

class Model {
public:
    Model(const std::string& filename) {
        load_obj(filename);
    }

    // Accessors
    int nverts() const { return verts_.size(); }
    int nfaces() const { return faces_.size(); }
    bool has_normals() const { return !normals_.empty(); }
    bool has_uvs() const { return !uvs_.empty(); }

    Vec3 vert(int i) const { return verts_[i]; }
    Vec3 normal(int iface, int nvert) const {
        int idx = faces_[iface].normal_indices[nvert];
        return normals_[idx];
    }
    Vec2 uv(int iface, int nvert) const {
        int idx = faces_[iface].uv_indices[nvert];
        return uvs_[idx];
    }

    // Get face vertex indices
    std::vector<int> face(int idx) const { return faces_[idx].vertex_indices; }

    // Texture maps — bilinear filtering for smoother results
    TGAColor diffuse(const Vec2& uv) const {
        if (diffusemap_.width() == 0) return TGAColor(255, 255, 255);
        double px = uv.x * (diffusemap_.width()  - 1);
        double py = uv.y * (diffusemap_.height() - 1);
        int x0 = static_cast<int>(px);
        int y0 = static_cast<int>(py);
        int x1 = std::min(x0 + 1, diffusemap_.width()  - 1);
        int y1 = std::min(y0 + 1, diffusemap_.height() - 1);
        double fx = px - x0, fy = py - y0;

        TGAColor c00 = diffusemap_.get(x0, y0);
        TGAColor c10 = diffusemap_.get(x1, y0);
        TGAColor c01 = diffusemap_.get(x0, y1);
        TGAColor c11 = diffusemap_.get(x1, y1);

        TGAColor result;
        for (int i = 0; i < 4; i++) {
            result[i] = static_cast<uint8_t>(
                c00[i] * (1 - fx) * (1 - fy) +
                c10[i] * fx       * (1 - fy) +
                c01[i] * (1 - fx) * fy        +
                c11[i] * fx       * fy
            );
        }
        return result;
    }

    Vec3 normal_from_map(const Vec2& uv) const {
        if (normalmap_.width() == 0) return Vec3(0, 0, 1);
        int x = static_cast<int>(uv.x * normalmap_.width());
        int y = static_cast<int>(uv.y * normalmap_.height());
        TGAColor c = normalmap_.get(x, y);
        // Map [0,255] to [-1,1]
        return Vec3(
            c[2] / 127.5 - 1.0,
            c[1] / 127.5 - 1.0,
            c[0] / 127.5 - 1.0
        );
    }

    double specular(const Vec2& uv) const {
        if (specularmap_.width() == 0) return 0.0;
        int x = static_cast<int>(uv.x * specularmap_.width());
        int y = static_cast<int>(uv.y * specularmap_.height());
        return specularmap_.get(x, y)[0] / 255.0;
    }

    // Texture loading
    void load_diffuse(const std::string& filename) {
        diffusemap_.read_tga_file(filename);
        diffusemap_.flip_vertically();
        std::cout << "  Loaded diffuse texture: " << filename << std::endl;
    }

    void load_normal_map(const std::string& filename) {
        normalmap_.read_tga_file(filename);
        normalmap_.flip_vertically();
        std::cout << "  Loaded normal map: " << filename << std::endl;
    }

    void load_specular_map(const std::string& filename) {
        specularmap_.read_tga_file(filename);
        specularmap_.flip_vertically();
        std::cout << "  Loaded specular map: " << filename << std::endl;
    }

private:
    struct Face {
        std::vector<int> vertex_indices;
        std::vector<int> uv_indices;
        std::vector<int> normal_indices;
    };

    std::vector<Vec3> verts_;
    std::vector<Vec2> uvs_;
    std::vector<Vec3> normals_;
    std::vector<Face> faces_;

    TGAImage diffusemap_;
    TGAImage normalmap_;
    TGAImage specularmap_;

    void load_obj(const std::string& filename) {
        FILE* file = fopen(filename.c_str(), "r");
        if (!file) {
            std::cerr << "Error: Cannot open OBJ file: " << filename << std::endl;
            return;
        }

        std::cout << "Loading model: " << filename << std::endl;

        char line[256];
        while (fgets(line, sizeof(line), file)) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") {
                // Vertex position
                Vec3 v;
                iss >> v.x >> v.y >> v.z;
                verts_.push_back(v);
            }
            else if (prefix == "vt") {
                // Texture coordinate
                Vec2 uv;
                iss >> uv.x >> uv.y;
                uvs_.push_back(uv);
            }
            else if (prefix == "vn") {
                // Vertex normal
                Vec3 n;
                iss >> n.x >> n.y >> n.z;
                normals_.push_back(n);
            }
            else if (prefix == "f") {
                // Face
                Face face;
                std::string vertex;

                while (iss >> vertex) {
                    // Parse formats: v, v/vt, v/vt/vn, v//vn
                    std::istringstream viss(vertex);
                    std::string index_str;
                    int indices[3] = {0, 0, 0};
                    int idx_count = 0;

                    while (std::getline(viss, index_str, '/') && idx_count < 3) {
                        if (!index_str.empty()) {
                            indices[idx_count] = std::stoi(index_str) - 1;  // Convert to 0-based
                        } else {
                            indices[idx_count] = -1;  // Missing index
                        }
                        idx_count++;
                    }

                    face.vertex_indices.push_back(indices[0]);
                    if (indices[1] >= 0) face.uv_indices.push_back(indices[1]);
                    if (indices[2] >= 0) face.normal_indices.push_back(indices[2]);
                }

                faces_.push_back(face);
            }
        }

        fclose(file);

        std::cout << "  Vertices: " << verts_.size() << std::endl;
        std::cout << "  Faces: " << faces_.size() << std::endl;
        std::cout << "  UVs: " << uvs_.size() << std::endl;
        std::cout << "  Normals: " << normals_.size() << std::endl;
    }
};

#endif
