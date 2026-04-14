#ifndef RT_SKY_H
#define RT_SKY_H

#include "../core/vec.h"
#include <cmath>
#include <algorithm>

// ============================================================
// Nishita Sky Model — Rayleigh + Mie atmospheric scattering
// Adapted from scratchapixel simulating-sky/skycolor.cpp
// ============================================================

namespace Sky {

// Physical constants
static const Vec3 betaR(3.8e-6, 13.5e-6, 33.1e-6);  // Rayleigh coefficients
static const double betaM_scalar = 21e-6;               // Mie coefficient
static const double Hr = 7994.0;                         // Rayleigh scale height
static const double Hm = 1200.0;                         // Mie scale height
static const double earthRadius = 6360e3;
static const double atmosphereRadius = 6420e3;

inline bool ray_sphere_intersect(const Vec3& orig, const Vec3& dir, double radius, double& t0, double& t1) {
    double a = dir * dir;
    double b = 2.0 * (orig * dir);
    double c = (orig * orig) - radius * radius;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0) return false;
    double sq = std::sqrt(disc);
    t0 = (-b - sq) / (2.0 * a);
    t1 = (-b + sq) / (2.0 * a);
    return true;
}

// Compute sky color for a given ray direction and sun direction
// sun_dir should be normalized, pointing toward the sun
inline Vec3 compute(const Vec3& ray_dir, const Vec3& sun_dir) {
    const int numSamples = 8;
    const int numSamplesLight = 4;

    Vec3 orig(0, earthRadius + 1.0, 0);  // observer at ground level

    double t0, t1;
    if (!ray_sphere_intersect(orig, ray_dir, atmosphereRadius, t0, t1) || t1 < 0)
        return Vec3(0, 0, 0);
    if (t0 < 0) t0 = 0;

    double segmentLength = (t1 - t0) / numSamples;
    double tCurrent = t0;

    Vec3 sumR(0, 0, 0), sumM(0, 0, 0);
    double opticalDepthR = 0, opticalDepthM = 0;

    double mu = ray_dir * sun_dir;
    double mu2 = mu * mu;

    // Rayleigh phase
    double phaseR = 3.0 / (16.0 * M_PI) * (1.0 + mu2);
    // Mie phase (Henyey-Greenstein, g=0.76)
    double g = 0.76;
    double phaseM = 3.0 / (8.0 * M_PI) * ((1.0 - g * g) * (1.0 + mu2))
                  / ((2.0 + g * g) * std::pow(1.0 + g * g - 2.0 * g * mu, 1.5));

    for (int i = 0; i < numSamples; i++) {
        Vec3 samplePos = orig + ray_dir * (tCurrent + segmentLength * 0.5);
        double height = samplePos.norm() - earthRadius;

        // Compute optical depth at this sample
        double hr = std::exp(-height / Hr) * segmentLength;
        double hm = std::exp(-height / Hm) * segmentLength;
        opticalDepthR += hr;
        opticalDepthM += hm;

        // Light ray from sample toward sun
        double tl0, tl1;
        ray_sphere_intersect(samplePos, sun_dir, atmosphereRadius, tl0, tl1);
        double segLenLight = tl1 / numSamplesLight;
        double optDepthLR = 0, optDepthLM = 0;
        bool shadow = false;
        for (int j = 0; j < numSamplesLight; j++) {
            Vec3 sampleLight = samplePos + sun_dir * (segLenLight * (j + 0.5));
            double hLight = sampleLight.norm() - earthRadius;
            if (hLight < 0) { shadow = true; break; }
            optDepthLR += std::exp(-hLight / Hr) * segLenLight;
            optDepthLM += std::exp(-hLight / Hm) * segLenLight;
        }
        if (!shadow) {
            // Attenuation along primary + light ray
            Vec3 tau(
                (opticalDepthR + optDepthLR) * betaR.x + (opticalDepthM + optDepthLM) * betaM_scalar * 1.1,
                (opticalDepthR + optDepthLR) * betaR.y + (opticalDepthM + optDepthLM) * betaM_scalar * 1.1,
                (opticalDepthR + optDepthLR) * betaR.z + (opticalDepthM + optDepthLM) * betaM_scalar * 1.1
            );
            Vec3 attenuation(std::exp(-tau.x), std::exp(-tau.y), std::exp(-tau.z));
            sumR = sumR + Vec3(attenuation.x * hr * betaR.x,
                               attenuation.y * hr * betaR.y,
                               attenuation.z * hr * betaR.z);
            sumM = sumM + Vec3(attenuation.x * hm * betaM_scalar,
                               attenuation.y * hm * betaM_scalar,
                               attenuation.z * hm * betaM_scalar);
        }
        tCurrent += segmentLength;
    }

    // Solar intensity (20 is a good default)
    double sunIntensity = 20.0;
    Vec3 result(
        (sumR.x * phaseR + sumM.x * phaseM) * sunIntensity,
        (sumR.y * phaseR + sumM.y * phaseM) * sunIntensity,
        (sumR.z * phaseR + sumM.z * phaseM) * sunIntensity
    );

    return result;
}

// Tone-map HDR sky to [0,1] — filmic curve from scratchapixel
inline Vec3 tonemap(const Vec3& hdr) {
    auto tm = [](double v) -> double {
        return (v < 1.413) ? std::pow(v * 0.38317, 1.0 / 2.2) : 1.0 - std::exp(-v);
    };
    return Vec3(
        std::min(1.0, std::max(0.0, tm(hdr.x))),
        std::min(1.0, std::max(0.0, tm(hdr.y))),
        std::min(1.0, std::max(0.0, tm(hdr.z)))
    );
}

} // namespace Sky

#endif
