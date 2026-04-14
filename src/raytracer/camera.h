#ifndef RT_CAMERA_H
#define RT_CAMERA_H

#include "../core/vec.h"
#include "ray.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Ray Tracing Camera (with depth of field)
// ============================================================

class RTCamera {
public:
    Vec3 origin;
    Vec3 lower_left_corner;
    Vec3 horizontal;
    Vec3 vertical;
    Vec3 u, v, w;
    double lens_radius;

    RTCamera(Vec3 lookfrom, Vec3 lookat, Vec3 vup,
             double vfov, double aspect_ratio,
             double aperture = 0.0, double focus_dist = 1.0) {

        double theta = vfov * M_PI / 180.0;
        double h = std::tan(theta / 2);
        double viewport_height = 2.0 * h;
        double viewport_width = aspect_ratio * viewport_height;

        w = normalized(lookfrom - lookat);
        u = normalized(cross(vup, w));
        v = cross(w, u);

        origin = lookfrom;
        horizontal = u * (viewport_width * focus_dist);
        vertical = v * (viewport_height * focus_dist);
        lower_left_corner = origin - horizontal/2.0 - vertical/2.0 - w*focus_dist;

        lens_radius = aperture / 2.0;
    }

    Ray get_ray(double s, double t) const {
        Vec3 rd = random_in_unit_disk() * lens_radius;
        Vec3 offset = u * rd.x + v * rd.y;

        return Ray(origin + offset,
                   lower_left_corner + horizontal*s + vertical*t - origin - offset);
    }
};

#endif
