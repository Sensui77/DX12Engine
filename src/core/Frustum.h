#pragma once
#include <DirectXMath.h>
#include <cmath>

// Axis-Aligned Bounding Box for frustum testing.
struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// Frustum defined by 6 planes extracted from the ViewProjection matrix.
// Each plane is stored as (A, B, C, D) where Ax + By + Cz + D >= 0 means inside.
struct Frustum {
    DirectX::XMFLOAT4 planes[6]; // Left, Right, Bottom, Top, Near, Far

    // Extract frustum planes from a ViewProjection matrix (row-major).
    // Uses the Gribb-Hartmann method.
    void ExtractFromVP(const DirectX::XMMATRIX& vp) {
        DirectX::XMFLOAT4X4 m;
        DirectX::XMStoreFloat4x4(&m, vp);

        // Left:   row3 + row0
        planes[0] = { m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41 };
        // Right:  row3 - row0
        planes[1] = { m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41 };
        // Bottom: row3 + row1
        planes[2] = { m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42 };
        // Top:    row3 - row1
        planes[3] = { m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42 };
        // Near:   row2
        planes[4] = { m._13, m._23, m._33, m._43 };
        // Far:    row3 - row2
        planes[5] = { m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43 };

        // Normalize all planes
        for (int i = 0; i < 6; i++) {
            float len = std::sqrt(
                planes[i].x * planes[i].x +
                planes[i].y * planes[i].y +
                planes[i].z * planes[i].z);
            if (len > 0.0001f) {
                planes[i].x /= len;
                planes[i].y /= len;
                planes[i].z /= len;
                planes[i].w /= len;
            }
        }
    }

    // Test if an AABB is entirely outside the frustum.
    // Returns true if visible (fully or partially inside).
    bool TestAABB(const AABB& box) const {
        for (int i = 0; i < 6; i++) {
            const auto& p = planes[i];

            // Find the "most positive" corner relative to the plane normal
            float px = (p.x >= 0.0f) ? box.maxX : box.minX;
            float py = (p.y >= 0.0f) ? box.maxY : box.minY;
            float pz = (p.z >= 0.0f) ? box.maxZ : box.minZ;

            // If the most positive corner is outside, the box is fully outside
            float dist = p.x * px + p.y * py + p.z * pz + p.w;
            if (dist < 0.0f) {
                return false;
            }
        }
        return true;
    }
};
