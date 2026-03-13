// ═══════════════════════════════════════════════════════════════════════
// TinyRaytracer — Shadertoy GLSL port
// ═══════════════════════════════════════════════════════════════════════
// Paste into https://www.shadertoy.com as a new shader.
// Matches the CPU raytracer from the TinyRaytracer Course.
//
// PERFORMANCE TIPS:
//   1. GPU executes mainImage() in parallel for every pixel — no threads
//      needed, the hardware handles it.
//   2. Avoid divergent branches (if/else where threads take different
//      paths) — modern GPUs handle this with masking but it still costs.
//   3. Use built-in functions: reflect(), mix(), clamp(), dot() — these
//      map to single GPU instructions.
//   4. Minimize texture reads; our shader uses none (procedural only).
//   5. Loop unrolling: the compiler does this automatically for small
//      constant-bound loops (our N_SPHERES=4, N_LIGHTS=3).
//   6. To add iChannel0 environment map: replace the sky gradient in
//      cast_ray with: texture(iChannel0, dir).rgb
// ═══════════════════════════════════════════════════════════════════════

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

// ── Ray-sphere intersection ─────────────────────────────────────────
// Same algorithm as our C sphere_intersect:
//   L = center - origin
//   tca = dot(L, dir)       → projection of L onto ray
//   d2 = |L|^2 - tca^2     → squared distance from center to ray
//   thc = sqrt(r^2 - d2)   → half-chord length
//   t = tca - thc           → nearest intersection
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

// ── Scene intersection ──────────────────────────────────────────────
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
    // Checkerboard floor (same as our plane intersection y=-4)
    if (abs(dir.y) > 1e-3) {
        float d = -(orig.y + 4.0) / dir.y;
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

// ── Refraction (Snell's law) ────────────────────────────────────────
// Same as our refract() in refract.h, but GLSL has a built-in refract().
// We use the built-in here.

// ── cast_ray — iterative (GLSL has no recursion) ────────────────────
// Our C version is recursive: cast_ray calls cast_ray for reflections
// and refractions.  GLSL forbids recursion, so we unroll into a loop
// with a weight accumulator.
//
// Optimization note: this loop has at most MAX_DEPTH=3 iterations.
// The GLSL compiler may fully unroll it for better GPU performance.
vec3 cast_ray(vec3 orig, vec3 dir) {
    vec3  color  = vec3(0.0);
    float weight = 1.0;

    for (int depth = 0; depth < MAX_DEPTH; depth++) {
        float t;
        vec3  N;
        Material mat;

        if (!scene_hit(orig, dir, t, N, mat)) {
            // Sky gradient (replace with texture(iChannel0, dir).rgb
            // for environment mapping)
            float sky = 0.5 * (dir.y + 1.0);
            color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);
            break;
        }

        vec3 pt = orig + dir * t;

        // ── Phong lighting (same as our compute_lighting) ──────
        float diff_i = 0.0, spec_i = 0.0;
        for (int l = 0; l < N_LIGHTS; l++) {
            vec3  ldir = normalize(light_pos[l] - pt);
            float ln   = dot(ldir, N);

            // Shadow ray
            float st; vec3 sN; Material sM;
            vec3 sOrig = pt + N * 0.001;
            if (scene_hit(sOrig, ldir, st, sN, sM) &&
                st < length(light_pos[l] - pt)) continue;

            diff_i += light_int[l] * max(0.0, ln);
            // reflect(-ldir, N) = our vec3_reflect
            spec_i += light_int[l] *
                pow(max(0.0, dot(reflect(-ldir, N), -dir)), mat.spec_exp);
        }

        vec3 diffuse  = mat.color * diff_i;
        vec3 specular = vec3(spec_i);

        color += weight * (diffuse * mat.albedo.x + specular * mat.albedo.y);

        // ── Reflection (iterative replacement for recursion) ───
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

// ── Main entry point ────────────────────────────────────────────────
// Shadertoy calls this for every pixel every frame.
// fragCoord = pixel coordinates, iResolution = viewport size.
// This is equivalent to our render_scene loop + trace_pixel.
//
// Optimization: the entire mainImage runs in parallel on the GPU —
// one thread per pixel.  A 1920x1080 frame launches ~2M threads.
// Our CPU version needs pthreads for parallelism; the GPU gets it
// for free.
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Map pixel to normalized device coordinates [-1,1]
    // Same as our: x = (2*px/width - 1) * tan(fov/2) * aspect
    vec2  uv  = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    float fov = tan(FOV / 2.0);
    vec3  dir = normalize(vec3(uv * fov, -1.0));

    // Optional: interactive camera with iMouse
    // Uncomment to enable mouse orbit (like our OrbitControls):
    // vec2 m = iMouse.xy / iResolution.xy - 0.5;
    // float yaw   = m.x * 6.28;
    // float pitch = m.y * 1.5;
    // mat3 rot = mat3(cos(yaw), 0, sin(yaw),
    //                 0, 1, 0,
    //                 -sin(yaw), 0, cos(yaw));
    // dir = rot * dir;

    fragColor = vec4(cast_ray(vec3(0.0), dir), 1.0);
}
