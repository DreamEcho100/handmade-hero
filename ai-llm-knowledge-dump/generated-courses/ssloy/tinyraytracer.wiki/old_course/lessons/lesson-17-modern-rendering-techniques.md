# Lesson 17 — Modern Rendering Techniques (Roadmap)

> **What you'll build:** This lesson has no new code. Instead, it maps 10 modern rendering techniques to the concepts you already understand, shows how each would extend our raytracer, and includes a complete GLSL fragment shader that ports `cast_ray` to Shadertoy.

## Observable outcome

No visual change to the running program. This lesson is a knowledge bridge — connecting the CPU raytracer you have built to the techniques used in production rendering engines (Blender Cycles, Unreal Engine Lumen, NVIDIA OptiX). After reading this lesson, you should be able to pick up any of these topics and understand where it fits in the pipeline you already know.

## New concepts

- 10 modern rendering techniques and how they relate to our codebase
- GLSL fragment shader — porting `cast_ray` to run on the GPU via Shadertoy
- The difference between offline (path tracing) and real-time (rasterization + RT) pipelines

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| (none) | — | This is a roadmap lesson with no source code changes |

## Background — the rendering landscape

Our raytracer implements the fundamentals that underpin all of modern rendering:

- **Rays** (L03) — the universal primitive for visibility queries
- **Phong shading** (L05-06) — a simple but effective lighting model
- **Shadows** (L07) — binary visibility from shadow rays
- **Reflections/refractions** (L08-09) — recursive ray tracing
- **Geometric intersection** (L03, L10, L13, L19) — the core of every ray tracer

Every technique below extends or replaces one of these foundations. The progression is: our raytracer (educational) -> path tracer (physically correct) -> GPU path tracer (real-time).

## The 10 techniques

### 1. Path Tracing

**What it is:** Instead of tracing one reflected ray per hit (Lesson 08), path tracing shoots hundreds of random rays per pixel, bouncing them through the scene. Each bounce follows a probabilistic distribution based on the material's BRDF (Bidirectional Reflectance Distribution Function). The average of all paths converges to the physically correct solution of the rendering equation.

**How it relates to our code:** In `cast_ray`, we compute `reflect_color` by tracing exactly one reflected ray. A path tracer would instead sample a random direction within the hemisphere above the hit point, weighted by the BRDF. Our `MAX_RECURSION_DEPTH` becomes a probabilistic termination (Russian roulette) rather than a hard cutoff.

**What would change:** `cast_ray` would take a random number generator parameter. Instead of `vec3_reflect(ray.direction, hit.normal)` for the single reflection direction, it would sample a cosine-weighted hemisphere direction. The pixel loop would average hundreds of samples per pixel.

**Resource:** "Ray Tracing in One Weekend" by Peter Shirley (free online) — extends exactly the architecture we built.

### 2. Bounding Volume Hierarchy (BVH)

**What it is:** A tree structure that spatially organizes scene geometry. Instead of testing every object (our `scene_intersect` loops over all spheres, all boxes, all triangles), a BVH tests the ray against bounding boxes at each tree level, pruning entire branches of geometry that the ray cannot possibly hit.

**How it relates to our code:** We already have the core idea in Lesson 16 — `aabb_test` before the per-cell loop is a one-level BVH. A full BVH is the same concept applied recursively: split geometry into two groups, wrap each group in an AABB, and recurse.

**What would change:** `scene_intersect` would be replaced by `bvh_intersect(ray, root_node)`. The `for` loop over all objects becomes a tree traversal. Construction happens at scene load time (similar to `voxel_model_init`).

**Resource:** "Physically Based Rendering" (PBRT) by Pharr, Jakob, and Humphreys — Chapter 4 covers BVH construction and traversal.

### 3. GPU Ray Tracing (RTX / Compute Shaders)

**What it is:** Running the ray tracing loop on the GPU instead of the CPU. Each pixel is a thread, and thousands of threads run in parallel. Modern GPUs (NVIDIA RTX) have dedicated hardware (RT cores) that accelerate BVH traversal and ray-triangle intersection.

**How it relates to our code:** The per-pixel loop in `render_scene` maps directly to GPU threads. Each thread runs the equivalent of `cast_ray` independently. The `scene_intersect` function becomes a hardware-accelerated BVH query.

**What would change:** The entire `render.c` loop would be replaced by a compute shader dispatch. Scene data (spheres, triangles, materials) would be uploaded as GPU buffers. The backbuffer write becomes a texture write.

**Resource:** "Ray Tracing Gems" (NVIDIA, free online) — practical GPU RT techniques with code.

### 4. Denoising

**What it is:** Path tracing produces noisy images when using too few samples per pixel. Denoising uses AI or statistical filters to clean up the noise. Modern denoisers (NVIDIA OptiX AI Denoiser, Intel OIDN) can produce clean images from as few as 1 sample per pixel by using auxiliary buffers (normals, albedo, depth).

**How it relates to our code:** Our `HitRecord` already stores `normal`, `point`, and `material_index` — exactly the auxiliary data that denoisers need. The depth buffer from Lesson 15 is another input.

**What would change:** After `render_scene_to_float`, a denoising pass would read the color buffer + normal buffer + albedo buffer and output a clean image. The denoiser is a post-process, not a change to the raytracing pipeline.

**Resource:** Intel Open Image Denoise (OIDN) — open-source, CPU-based, easy to integrate.

### 5. Physically Based Rendering (PBR)

**What it is:** Replacing our ad-hoc Phong model with physically motivated material descriptions. PBR materials use metalness, roughness, and albedo instead of our `albedo[4]` weights. The Cook-Torrance microfacet BRDF replaces the `pow(cos, specular_exponent)` Phong specular.

**How it relates to our code:** Our `RtMaterial` struct would be replaced. The `compute_lighting` function in `lighting.c` would use the Cook-Torrance formula instead of the Phong formula. The albedo weights `[diffuse, specular, reflect, refract]` would be derived from metalness and roughness.

**What would change:** `RtMaterial` becomes `{ Vec3 albedo; float metalness; float roughness; float ior; }`. `compute_lighting` gains a GGX normal distribution function, a Smith geometry function, and a Fresnel-Schlick approximation.

**Resource:** "Real-Time Rendering" by Akenine-Moller et al., Chapter 9 — definitive PBR reference.

### 6. Importance Sampling

**What it is:** In path tracing, randomly sampling the hemisphere wastes many samples on directions that contribute little light. Importance sampling biases the random distribution toward directions that matter — toward light sources (direct illumination) or along the BRDF lobe (specular reflections).

**How it relates to our code:** Our `compute_lighting` already loops over all lights and computes their direct contribution. Importance sampling is the probabilistic equivalent: instead of testing all lights, randomly pick one light with probability proportional to its contribution.

**What would change:** The light loop in `compute_lighting` would be replaced by a single randomly-chosen light, scaled by 1/probability. The reflection direction would be sampled from the BRDF distribution instead of using the perfect mirror direction.

**Resource:** "Physically Based Rendering" (PBRT) — Chapters 13-14 cover Monte Carlo integration and importance sampling.

### 7. Ambient Occlusion (AO)

**What it is:** A soft shadow technique that darkens crevices, corners, and areas where geometry is close together. It approximates how much of the hemisphere above a point is blocked by nearby geometry.

**How it relates to our code:** Our shadow rays (Lesson 07) test visibility to specific light sources. AO shoots random rays into the hemisphere and counts how many are blocked by nearby geometry — it is shadow testing without a specific light.

**What would change:** After computing `diffuse_color` in `cast_ray`, multiply by an AO factor. The AO factor is computed by tracing N random rays from `hit.point + epsilon * hit.normal` into the hemisphere and counting what fraction hit geometry within a short distance.

**Resource:** "Ambient Occlusion" chapter in "GPU Gems" (NVIDIA, free online).

### 8. Texture Mapping

**What it is:** Replacing the constant `diffuse_color` in `RtMaterial` with a 2D image lookup. Each point on a surface has UV coordinates that map to a pixel in the texture image.

**How it relates to our code:** We already have the core idea in two places: (1) the checkerboard floor (Lesson 10) uses `hit.point.x` and `hit.point.z` as implicit UV coordinates, and (2) the environment map (Lesson 18) uses spherical UV mapping. Texture mapping applies the same concept to arbitrary surfaces.

**What would change:** `RtMaterial` gains a texture pointer and UV mapping function. `cast_ray` looks up `mat.diffuse_color` from the texture at the hit point's UV coordinates instead of using a constant color.

**Resource:** "Fundamentals of Computer Graphics" by Marschner and Shirley — Chapter 11 covers texture mapping.

### 9. Normal Mapping

**What it is:** Perturbing the surface normal at each hit point using a texture (normal map), creating the illusion of surface detail without adding geometric complexity. Bumps, scratches, and brick patterns are all achievable with normal maps.

**How it relates to our code:** `hit.normal` in our `HitRecord` is computed from geometry (sphere center, box axis, triangle interpolation). Normal mapping modifies this normal after intersection but before lighting. The lighting code (`compute_lighting`) does not need to change — it already uses `hit.normal`.

**What would change:** After `scene_intersect` fills `hit.normal`, a tangent-space normal map lookup would perturb the normal. This requires UV coordinates (see Texture Mapping) and a tangent/bitangent basis at the hit point.

**Resource:** "Learn OpenGL" by Joey de Vries (free online) — Normal Mapping chapter.

### 10. Depth of Field (DOF)

**What it is:** Simulating camera lens blur. Objects at the focal distance are sharp; objects closer or farther are blurry. This is achieved by jittering the ray origin across a virtual lens aperture while keeping the focal point fixed.

**How it relates to our code:** Our camera (Lesson 02-03) traces one ray per pixel from a point origin. DOF replaces this with multiple rays per pixel, each originating from a random point on a lens disk, all converging at the focal plane. The average of these rays produces blur for out-of-focus objects.

**What would change:** In `render_scene`, the per-pixel ray generation would sample a random point on a disk of radius `aperture` centered at `cam->position`. The ray direction would be adjusted to pass through the same focal point. Multiple samples per pixel are averaged.

**Resource:** "Ray Tracing in One Weekend" by Peter Shirley — Chapter 12 covers defocus blur.

## GLSL Shadertoy port — `cast_ray` on the GPU

ssloy's Part 3 shows how the same raytracer runs as a GLSL fragment shader on Shadertoy. Below is a complete port of our `cast_ray` logic to GLSL, showing how each C concept maps to shader code.

```glsl
// ── TinyRaytracer — Shadertoy port (from ssloy Part 3) ──────────────
// Paste this into shadertoy.com as a new shader.
// This replicates our C raytracer's core: spheres, Phong, reflections.

const int   MAX_DEPTH   = 3;
const float FOV         = 1.0472; // PI/3
const vec3  BG_TOP      = vec3(0.2, 0.7, 0.8);
const vec3  BG_BOT      = vec3(1.0, 1.0, 1.0);

// ── Materials ────────────────────────────────────────────────────────
struct Material {
    vec3  color;
    vec4  albedo;  // diffuse, specular, reflect, refract
    float spec_exp;
    float ior;
};

const Material ivory   = Material(vec3(0.4,0.4,0.3), vec4(0.6,0.3,0.1,0.0), 50.0, 1.0);
const Material rubber  = Material(vec3(0.3,0.1,0.1), vec4(0.9,0.1,0.0,0.0), 10.0, 1.0);
const Material mirror  = Material(vec3(1.0,1.0,1.0), vec4(0.0,10.,0.8,0.0), 1425., 1.0);
const Material glass   = Material(vec3(0.6,0.7,0.8), vec4(0.0,0.5,0.1,0.8), 125., 1.5);

// ── Spheres ─────────────────────────────────────────────────────────
const int N_SPHERES = 4;
const vec4 spheres[4] = vec4[4](       // xyz = center, w = radius
    vec4(-3.0, 0.0,-16.0, 2.0),
    vec4(-1.0,-1.5,-12.0, 2.0),
    vec4( 1.5,-0.5,-18.0, 3.0),
    vec4( 7.0, 5.0,-18.0, 4.0)
);
const int sphere_mat[4] = int[4](0, 3, 1, 2); // material indices

// ── Lights ──────────────────────────────────────────────────────────
const int N_LIGHTS = 3;
const vec3 light_pos[3] = vec3[3](
    vec3(-20., 20., 20.),
    vec3( 30., 50.,-25.),
    vec3( 30., 20., 30.)
);
const float light_int[3] = float[3](1.5, 1.8, 1.7);

// ── Ray-sphere intersection (same as our sphere_intersect) ──────────
float sphere_hit(vec3 orig, vec3 dir, vec4 sph) {
    vec3  L   = sph.xyz - orig;
    float tca = dot(L, dir);
    float d2  = dot(L, L) - tca * tca;
    float r2  = sph.w * sph.w;
    if (d2 > r2) return -1.0;
    float thc = sqrt(r2 - d2);
    float t0  = tca - thc;
    float t1  = tca + thc;
    if (t0 < 0.0) t0 = t1;
    if (t0 < 0.0) return -1.0;
    return t0;
}

// ── Scene intersection (same as our scene_intersect) ────────────────
bool scene_hit(vec3 orig, vec3 dir,
               out float t, out vec3 N, out Material mat) {
    t = 1e10;
    // Spheres
    for (int i = 0; i < N_SPHERES; i++) {
        float d = sphere_hit(orig, dir, spheres[i]);
        if (d > 0.001 && d < t) {
            t = d;
            vec3 pt = orig + dir * d;
            N   = normalize(pt - spheres[i].xyz);
            mat = (sphere_mat[i]==0) ? ivory :
                  (sphere_mat[i]==1) ? rubber :
                  (sphere_mat[i]==2) ? mirror : glass;
        }
    }
    // Checkerboard floor
    if (abs(dir.y) > 1e-3) {
        float d = -orig.y / dir.y;
        if (d > 0.001 && d < t) {
            vec3 pt = orig + dir * d;
            if (abs(pt.x) < 30.0 && abs(pt.z) < 50.0) {
                t = d;
                N = vec3(0., 1., 0.);
                int checker = int(floor(pt.x) + floor(pt.z)) & 1;
                mat = Material(
                    checker==1 ? vec3(0.3) : vec3(0.3,0.2,0.1),
                    vec4(1.,0.,0.,0.), 1.0, 1.0);
            }
        }
    }
    return t < 1e9;
}

// ── cast_ray (same as our cast_ray, iterative for GLSL) ─────────────
vec3 cast_ray(vec3 orig, vec3 dir) {
    // GLSL doesn't support true recursion.  Unroll to MAX_DEPTH iterations.
    // We store partial results and accumulate reflection/refraction weights.
    vec3  color = vec3(0.0);
    float weight = 1.0;

    for (int depth = 0; depth < MAX_DEPTH; depth++) {
        float t;
        vec3  N;
        Material mat;

        if (!scene_hit(orig, dir, t, N, mat)) {
            float sky = 0.5 * (dir.y + 1.0);
            color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);
            break;
        }

        vec3 pt = orig + dir * t;

        // Diffuse + specular (Phong)
        float diff_i = 0.0, spec_i = 0.0;
        for (int l = 0; l < N_LIGHTS; l++) {
            vec3  ldir = normalize(light_pos[l] - pt);
            float ln   = dot(ldir, N);

            // Shadow
            float st; vec3 sN; Material sM;
            vec3 sOrig = pt + N * 0.001;
            if (scene_hit(sOrig, ldir, st, sN, sM) &&
                st < length(light_pos[l] - pt)) continue;

            diff_i += light_int[l] * max(0.0, ln);
            spec_i += light_int[l] *
                pow(max(0.0, dot(reflect(-ldir, N), -dir)), mat.spec_exp);
        }

        vec3 diffuse  = mat.color * diff_i;
        vec3 specular = vec3(spec_i);

        color += weight * (diffuse * mat.albedo.x + specular * mat.albedo.y);

        // Reflection — continue tracing with reduced weight
        if (mat.albedo.z > 0.0) {
            weight *= mat.albedo.z;
            dir  = reflect(dir, N);
            orig = pt + N * 0.001;
        } else {
            break;
        }
    }
    return color;
}

// ── Main (Shadertoy entry point) ────────────────────────────────────
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2  uv  = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    float fov = tan(FOV / 2.0);
    vec3  dir = normalize(vec3(uv * fov, -1.0));
    fragColor = vec4(cast_ray(vec3(0.0), dir), 1.0);
}
```

**Key differences from our C code:**
- GLSL has no recursion — reflections use an iterative loop with a `weight` accumulator
- `reflect()` is a built-in GLSL function (we use `vec3_reflect` in `refract.h`)
- `mix(a, b, t)` is GLSL's lerp (we use `vec3_lerp`)
- Materials are stored as constants, not in a scene struct
- `out` parameters replace our `HitRecord` struct (GLSL has no structs-by-pointer the same way)

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Shadertoy shader is all black | `mainImage` not producing valid colors | Check that `cast_ray` returns positive values; ensure `dir` is normalized |
| GLSL compile error: "recursion not allowed" | Tried to call `cast_ray` from within `cast_ray` | GLSL does not support recursion; use the iterative loop pattern shown above |
| Reflections missing in GLSL port | Forgot the iterative reflection loop | The `weight *= mat.albedo.z` accumulation replaces recursive calls |
| Confusing PBR with Phong | Trying to use roughness/metalness in our Phong pipeline | PBR requires replacing `compute_lighting` entirely; it is a different model |
| "Which technique should I learn next?" | Analysis paralysis | Start with Path Tracing (technique 1) — it directly extends `cast_ray` |

## Exercise

> Copy the GLSL shader above into [shadertoy.com](https://www.shadertoy.com) and verify it produces the same scene as our C raytracer. Then add a box to the GLSL version using the per-face algorithm from Lesson 13 — you will need to translate `box_intersect` to GLSL.

## JS ↔ C concept map

| JS / Web concept | C / GLSL equivalent in this lesson | Key difference |
|---|---|---|
| WebGL fragment shader | GLSL `void mainImage(...)` | Same language (GLSL); different entry point |
| `requestAnimationFrame` loop | Shadertoy runs `mainImage` every frame automatically | GPU executes once per pixel per frame |
| `class Material { ... }` | GLSL `struct Material { ... }` | GLSL structs are value types, no methods |
| `Reflect.reflect(...)` | GLSL built-in `reflect(I, N)` | GLSL has reflection as a primitive |
| Three.js / Babylon.js scene graph | Hardcoded arrays in GLSL | No scene graph on GPU; everything is constants or buffers |
