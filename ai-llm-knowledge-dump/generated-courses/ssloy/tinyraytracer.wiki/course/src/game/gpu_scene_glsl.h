#ifndef GAME_GPU_SCENE_GLSL_H
#define GAME_GPU_SCENE_GLSL_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/gpu_scene_glsl.h — TinyRaytracer Course (L24-L26)
 * ═══════════════════════════════════════════════════════════════════════════
 * Complete data-driven GLSL raytracer body for in-process GPU rendering.
 * Receives ALL scene data via uniforms + textures (no hardcoded constants).
 *
 * This replaces GLSL_RAYTRACER_SOURCE for in-process rendering.
 * The original shader_glsl.h is preserved for Shadertoy export (Shift+L).
 *
 * UNIFORM INTERFACE:
 *   Materials (16 max): uMatColor[], uMatAlbedo[], uMatSpec[], uMatIOR[]
 *   Spheres (16 max):   uSphere[] (vec4: xyz=center, w=radius), uSphereMat[]
 *   Lights (8 max):     uLight[] (vec4: xyz=pos, w=intensity)
 *   Boxes (8 max):      uBoxCenter[], uBoxHalf[], uBoxMat[]
 *   Voxels (4 max):     uVoxPos[], uVoxScale[], uVoxBitfield[] (5 uint per model)
 *   Meshes (4 max):     uMeshTriTex (sampler2D), uMeshTriCount[], uMeshDataOff[]
 *   EnvMaps:            uEnvEquirect (sampler2D), uEnvCube (samplerCube)
 *   Toggles:            uFeatures (int bitfield)
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* ── Scene uniform declarations (inserted into preamble) ──────────────── */
#define GPU_SCENE_UNIFORMS                                                     \
  "// ── Scene uniforms (L24-L26) ───────────────────────────────────\n"       \
  "uniform int   uMatCount;\n"                                                 \
  "uniform vec3  uMatColor[16];\n"                                             \
  "uniform vec4  uMatAlbedo[16];\n"                                            \
  "uniform float uMatSpec[16];\n"                                              \
  "uniform float uMatIOR[16];\n"                                               \
  "\n"                                                                         \
  "uniform int   uSphereCount;\n"                                              \
  "uniform vec4  uSphere[16];\n"                                               \
  "uniform int   uSphereMat[16];\n"                                            \
  "\n"                                                                         \
  "uniform int   uLightCount;\n"                                               \
  "uniform vec4  uLight[8];\n"                                                 \
  "\n"                                                                         \
  "uniform int   uBoxCount;\n"                                                 \
  "uniform vec3  uBoxCenter[8];\n"                                             \
  "uniform vec3  uBoxHalf[8];\n"                                               \
  "uniform int   uBoxMat[8];\n"                                                \
  "\n"                                                                         \
  "uniform int   uVoxCount;\n"                                                 \
  "uniform vec3  uVoxPos[4];\n"                                                \
  "uniform float uVoxScale[4];\n"                                              \
  "uniform int   uVoxMat[4];\n"                                                \
  "uniform vec3  uVoxBBoxCenter[4];\n"                                         \
  "uniform vec3  uVoxBBoxHalf[4];\n"                                           \
  "uniform int   uVoxSolidCount[4];\n"                                         \
  "uniform uint  uVoxBitfield[20];\n"                                          \
  "\n"                                                                         \
  "uniform int   uMeshCount;\n"                                                \
  "uniform vec3  uMeshPos[4];\n"                                               \
  "uniform vec3  uMeshBBoxCenter[4];\n"                                        \
  "uniform vec3  uMeshBBoxHalf[4];\n"                                          \
  "uniform int   uMeshMat[4];\n"                                               \
  "uniform int   uMeshTriCount[4];\n"                                          \
  "uniform int   uMeshDataOff[4];\n"                                           \
  "uniform sampler2D uMeshTriTex;\n"                                           \
  "\n"                                                                         \
  "uniform int   uEnvMode;\n"                                                  \
  "uniform sampler2D uEnvEquirect;\n"                                          \
  "uniform samplerCube uEnvCube;\n"                                            \
  "\n"                                                                         \
  "uniform int   uFeatures;\n"                                                 \
  "\n"

/* ── Complete data-driven GLSL raytracer body ─────────────────────────── */
static const char GPU_SCENE_GLSL_SOURCE[] =

/* ── Constants ───────────────────────────────────────────────────────── */
"const int   MAX_DEPTH   = 4;\n"
"const float PI          = 3.14159265;\n"
"const vec3  BG_TOP      = vec3(0.2, 0.7, 0.8);\n"
"const vec3  BG_BOT      = vec3(1.0, 1.0, 1.0);\n"
"const float VOXEL_SIZE  = 0.10;\n"
"const int   VOX_W = 6, VOX_H = 6, VOX_D = 4;\n"
"\n"

/* ── Material lookup ─────────────────────────────────────────────────── */
"struct Material {\n"
"    vec3  color;\n"
"    vec4  albedo;\n"
"    float spec_exp;\n"
"    float ior;\n"
"};\n"
"\n"
"Material get_material(int idx) {\n"
"    return Material(uMatColor[idx], uMatAlbedo[idx],\n"
"                    uMatSpec[idx], uMatIOR[idx]);\n"
"}\n"
"\n"

/* ── Feature test helpers ────────────────────────────────────────────── */
"bool feat_voxels()  { return (uFeatures & 0x01) != 0; }\n"
"bool feat_floor()   { return (uFeatures & 0x02) != 0; }\n"
"bool feat_boxes()   { return (uFeatures & 0x04) != 0; }\n"
"bool feat_meshes()  { return (uFeatures & 0x08) != 0; }\n"
"bool feat_reflect() { return (uFeatures & 0x10) != 0; }\n"
"bool feat_refract() { return (uFeatures & 0x20) != 0; }\n"
"bool feat_shadows() { return (uFeatures & 0x40) != 0; }\n"
"bool feat_envmap()  { return (uFeatures & 0x80) != 0; }\n"
"\n"

/* ── Ray-sphere intersection ─────────────────────────────────────────── */
"float sphere_hit(vec3 orig, vec3 dir, vec4 sph) {\n"
"    vec3  L   = sph.xyz - orig;\n"
"    float tca = dot(L, dir);\n"
"    float d2  = dot(L, L) - tca * tca;\n"
"    float r2  = sph.w * sph.w;\n"
"    if (d2 > r2) return -1.0;\n"
"    float thc = sqrt(r2 - d2);\n"
"    float t0  = tca - thc;\n"
"    float t1  = tca + thc;\n"
"    if (t0 < 0.001) t0 = t1;\n"
"    if (t0 < 0.001) return -1.0;\n"
"    return t0;\n"
"}\n"
"\n"

/* ── AABB slab test (bool only, for early-out) ───────────────────────── */
"bool aabb_test(vec3 orig, vec3 dir, vec3 center, vec3 half_sz) {\n"
"    vec3 bmin = center - half_sz;\n"
"    vec3 bmax = center + half_sz;\n"
"    vec3 invD = 1.0 / dir;\n"
"    vec3 t1 = (bmin - orig) * invD;\n"
"    vec3 t2 = (bmax - orig) * invD;\n"
"    vec3 tmin = min(t1, t2);\n"
"    vec3 tmax = max(t1, t2);\n"
"    float tNear = max(max(tmin.x, tmin.y), tmin.z);\n"
"    float tFar  = min(min(tmax.x, tmax.y), tmax.z);\n"
"    return tFar >= max(tNear, 0.0);\n"
"}\n"
"\n"

/* ── Per-face ray-box intersection (returns t, sets normal) ──────────── */
"float box_hit(vec3 orig, vec3 dir, vec3 center, vec3 half_sz, out vec3 N) {\n"
"    float best_t = 1e10;\n"
"    N = vec3(0.0);\n"
"    for (int axis = 0; axis < 3; axis++) {\n"
"        if (abs(dir[axis]) < 1e-5) continue;\n"
"        for (int side = -1; side <= 1; side += 2) {\n"
"            float face = center[axis] + float(side) * half_sz[axis];\n"
"            float t = (face - orig[axis]) / dir[axis];\n"
"            if (t < 0.001 || t >= best_t) continue;\n"
"            vec3 pt = orig + dir * t;\n"
"            int a1 = (axis + 1) % 3;\n"
"            int a2 = (axis + 2) % 3;\n"
"            if (pt[a1] >= center[a1] - half_sz[a1] &&\n"
"                pt[a1] <= center[a1] + half_sz[a1] &&\n"
"                pt[a2] >= center[a2] - half_sz[a2] &&\n"
"                pt[a2] <= center[a2] + half_sz[a2]) {\n"
"                best_t = t;\n"
"                N = vec3(0.0);\n"
"                N[axis] = float(side);\n"
"            }\n"
"        }\n"
"    }\n"
"    return best_t < 1e9 ? best_t : -1.0;\n"
"}\n"
"\n"

/* ── Voxel bitfield test ─────────────────────────────────────────────── */
"bool voxel_solid(int model_idx, int cell_id) {\n"
"    uint word = uVoxBitfield[model_idx * 5 + cell_id / 32];\n"
"    return (word & (1u << uint(cell_id & 31))) != 0u;\n"
"}\n"
"\n"

/* ── Voxel color hash (deterministic palette per cell) ───────────────── */
"vec3 voxel_color_from_id(int cell_id) {\n"
"    uint h = uint(cell_id);\n"
"    h = ((h >> 16u) ^ h) * 0x45d9f3bu;\n"
"    h = ((h >> 16u) ^ h) * 0x45d9f3bu;\n"
"    h = (h >> 16u) ^ h;\n"
"    float r = float((h >> 0u) & 0xFFu) / 255.0 * 0.6 + 0.2;\n"
"    float g = float((h >> 8u) & 0xFFu) / 255.0 * 0.6 + 0.2;\n"
"    float b = float((h >> 16u) & 0xFFu) / 255.0 * 0.6 + 0.2;\n"
"    return vec3(r, g, b);\n"
"}\n"
"\n"

/* ── Voxel model intersection ────────────────────────────────────────── */
"bool voxel_model_hit(vec3 orig, vec3 dir, int mi,\n"
"                     out float best_t, out vec3 best_N, out vec3 voxColor) {\n"
"    best_t = 1e10;\n"
"    voxColor = vec3(-1.0);\n"
"    if (!aabb_test(orig, dir, uVoxBBoxCenter[mi], uVoxBBoxHalf[mi]))\n"
"        return false;\n"
"    float vox_size = VOXEL_SIZE * uVoxScale[mi];\n"
"    float half_cell = vox_size * 0.5;\n"
"    vec3 hs = vec3(half_cell);\n"
"    bool found = false;\n"
"    for (int k = 0; k < VOX_D; k++) {\n"
"        for (int j = 0; j < VOX_H; j++) {\n"
"            for (int i = 0; i < VOX_W; i++) {\n"
"                int cell_id = i + j * VOX_W + k * VOX_W * VOX_H;\n"
"                if (!voxel_solid(mi, cell_id)) continue;\n"
"                vec3 cell_center = uVoxPos[mi] + vec3(\n"
"                    (float(i) - 3.0 + 0.5) * vox_size,\n"
"                    (float(j) - 3.0 + 0.5) * vox_size,\n"
"                    (-float(k) + 2.0 - 0.5) * vox_size);\n"
"                vec3 cn;\n"
"                float ct = box_hit(orig, dir, cell_center, hs, cn);\n"
"                if (ct > 0.0 && ct < best_t) {\n"
"                    best_t = ct;\n"
"                    best_N = cn;\n"
"                    voxColor = voxel_color_from_id(cell_id);\n"
"                    found = true;\n"
"                }\n"
"            }\n"
"        }\n"
"    }\n"
"    return found;\n"
"}\n"
"\n"

/* ── Mesh triangle texture read ──────────────────────────────────────── */
"vec3 read_tri_vec3(int texel_idx) {\n"
"    float u = (float(texel_idx) + 0.5) / float(textureSize(uMeshTriTex, 0).x);\n"
"    return texture(uMeshTriTex, vec2(u, 0.5)).xyz;\n"
"}\n"
"\n"

/* ── Moller-Trumbore ray-triangle intersection ───────────────────────── */
"float tri_hit(vec3 orig, vec3 dir,\n"
"              vec3 v0, vec3 v1, vec3 v2,\n"
"              vec3 n0, vec3 n1, vec3 n2,\n"
"              out vec3 N) {\n"
"    vec3 e1 = v1 - v0, e2 = v2 - v0;\n"
"    vec3 h = cross(dir, e2);\n"
"    float a = dot(e1, h);\n"
"    if (abs(a) < 1e-7) return -1.0;\n"
"    float f = 1.0 / a;\n"
"    vec3 s = orig - v0;\n"
"    float u = f * dot(s, h);\n"
"    if (u < 0.0 || u > 1.0) return -1.0;\n"
"    vec3 q = cross(s, e1);\n"
"    float v = f * dot(dir, q);\n"
"    if (v < 0.0 || u + v > 1.0) return -1.0;\n"
"    float t = f * dot(e2, q);\n"
"    if (t < 0.001) return -1.0;\n"
"    float w = 1.0 - u - v;\n"
"    N = normalize(n0 * w + n1 * u + n2 * v);\n"
"    return t;\n"
"}\n"
"\n"

/* ── Mesh intersection ───────────────────────────────────────────────── */
"bool mesh_hit(vec3 orig, vec3 dir, int mi,\n"
"              out float best_t, out vec3 best_N) {\n"
"    best_t = 1e10;\n"
"    if (!aabb_test(orig, dir, uMeshBBoxCenter[mi], uMeshBBoxHalf[mi]))\n"
"        return false;\n"
"    bool found = false;\n"
"    int off = uMeshDataOff[mi];\n"
"    for (int ti = 0; ti < uMeshTriCount[mi]; ti++) {\n"
"        int base = off + ti * 6;\n"
"        vec3 v0 = read_tri_vec3(base + 0);\n"
"        vec3 v1 = read_tri_vec3(base + 1);\n"
"        vec3 v2 = read_tri_vec3(base + 2);\n"
"        vec3 n0 = read_tri_vec3(base + 3);\n"
"        vec3 n1 = read_tri_vec3(base + 4);\n"
"        vec3 n2 = read_tri_vec3(base + 5);\n"
"        vec3 tn;\n"
"        float tt = tri_hit(orig, dir, v0,v1,v2, n0,n1,n2, tn);\n"
"        if (tt > 0.0 && tt < best_t) {\n"
"            best_t = tt;\n"
"            best_N = tn;\n"
"            found = true;\n"
"        }\n"
"    }\n"
"    return found;\n"
"}\n"
"\n"

/* ── Environment map sampling ────────────────────────────────────────── */
"vec3 envmap_procedural(vec3 dir) {\n"
"    float t = 0.5 * (dir.y + 1.0);\n"
"    vec3 ground  = vec3(0.35, 0.25, 0.15);\n"
"    vec3 horizon = vec3(0.85, 0.85, 0.80);\n"
"    vec3 zenith  = vec3(0.15, 0.35, 0.65);\n"
"    if (t < 0.5) return mix(ground, horizon, t * 2.0);\n"
"    return mix(horizon, zenith, (t - 0.5) * 2.0);\n"
"}\n"
"\n"
"vec3 envmap_equirect(vec3 dir) {\n"
"    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);\n"
"    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;\n"
"    return texture(uEnvEquirect, vec2(u, v)).rgb;\n"
"}\n"
"\n"
"vec3 envmap_cubemap(vec3 dir) {\n"
"    return texture(uEnvCube, dir).rgb;\n"
"}\n"
"\n"
"vec3 envmap_sample(vec3 dir) {\n"
"    if (uEnvMode == 1) return envmap_equirect(dir);\n"
"    if (uEnvMode == 2) return envmap_cubemap(dir);\n"
"    return envmap_procedural(dir);\n"
"}\n"
"\n"

/* ── Scene intersection ──────────────────────────────────────────────── */
"bool scene_hit(vec3 orig, vec3 dir,\n"
"               out float t, out vec3 N, out Material mat,\n"
"               out vec3 voxColor) {\n"
"    t = 1e10;\n"
"    voxColor = vec3(-1.0);\n"
"    bool found = false;\n"
"\n"
"    // ── Spheres ──────────────────────────────────────────────────\n"
"    for (int i = 0; i < uSphereCount; i++) {\n"
"        float d = sphere_hit(orig, dir, uSphere[i]);\n"
"        if (d > 0.001 && d < t) {\n"
"            t = d;\n"
"            N = normalize(orig + dir * d - uSphere[i].xyz);\n"
"            mat = get_material(uSphereMat[i]);\n"
"            voxColor = vec3(-1.0);\n"
"            found = true;\n"
"        }\n"
"    }\n"
"\n"
"    // ── Boxes ────────────────────────────────────────────────────\n"
"    if (feat_boxes()) {\n"
"        for (int i = 0; i < uBoxCount; i++) {\n"
"            vec3 bn;\n"
"            float bd = box_hit(orig, dir, uBoxCenter[i], uBoxHalf[i], bn);\n"
"            if (bd > 0.001 && bd < t) {\n"
"                t = bd;\n"
"                N = bn;\n"
"                mat = get_material(uBoxMat[i]);\n"
"                voxColor = vec3(-1.0);\n"
"                found = true;\n"
"            }\n"
"        }\n"
"    }\n"
"\n"
"    // ── Voxels ───────────────────────────────────────────────────\n"
"    if (feat_voxels()) {\n"
"        for (int i = 0; i < uVoxCount; i++) {\n"
"            float vt; vec3 vn, vc;\n"
"            if (voxel_model_hit(orig, dir, i, vt, vn, vc) && vt < t) {\n"
"                t = vt;\n"
"                N = vn;\n"
"                mat = get_material(uVoxMat[i]);\n"
"                voxColor = vc;\n"
"                found = true;\n"
"            }\n"
"        }\n"
"    }\n"
"\n"
"    // ── Meshes ───────────────────────────────────────────────────\n"
"    if (feat_meshes()) {\n"
"        for (int i = 0; i < uMeshCount; i++) {\n"
"            float mt; vec3 mn;\n"
"            if (mesh_hit(orig, dir, i, mt, mn) && mt < t) {\n"
"                t = mt;\n"
"                N = mn;\n"
"                mat = get_material(uMeshMat[i]);\n"
"                voxColor = vec3(-1.0);\n"
"                found = true;\n"
"            }\n"
"        }\n"
"    }\n"
"\n"
"    // ── Checkerboard floor (y=0) ────────────────────────────────\n"
"    if (feat_floor() && abs(dir.y) > 1e-3) {\n"
"        float fd = -orig.y / dir.y;\n"
"        if (fd > 0.001 && fd < t) {\n"
"            vec3 pt = orig + dir * fd;\n"
"            if (abs(pt.x) < 30.0 && abs(pt.z) < 50.0) {\n"
"                t = fd;\n"
"                N = vec3(0.0, 1.0, 0.0);\n"
"                int checker = int(floor(pt.x) + floor(pt.z)) & 1;\n"
"                mat = Material(\n"
"                    checker == 1 ? vec3(0.3) : vec3(0.3, 0.2, 0.1),\n"
"                    vec4(1.0, 0.0, 0.0, 0.0), 1.0, 1.0);\n"
"                voxColor = vec3(-1.0);\n"
"                found = true;\n"
"            }\n"
"        }\n"
"    }\n"
"\n"
"    return found;\n"
"}\n"
"\n"

/* ── cast_ray — iterative with weight accumulation ───────────────────── */
"vec3 cast_ray(vec3 orig, vec3 dir) {\n"
"    vec3  color  = vec3(0.0);\n"
"    float weight = 1.0;\n"
"    bool  exhausted = true; // true if loop runs out of bounces\n"
"\n"
"    for (int depth = 0; depth < MAX_DEPTH; depth++) {\n"
"        float t;\n"
"        vec3  N;\n"
"        Material mat;\n"
"        vec3  voxColor;\n"
"\n"
"        if (!scene_hit(orig, dir, t, N, mat, voxColor)) {\n"
"            // Background: envmap or sky gradient\n"
"            if (feat_envmap())\n"
"                color += weight * envmap_sample(dir);\n"
"            else {\n"
"                float sky = 0.5 * (dir.y + 1.0);\n"
"                color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);\n"
"            }\n"
"            exhausted = false;\n"
"            break;\n"
"        }\n"
"\n"
"        // Voxel per-cell color override\n"
"        vec3 diffuse_color = (voxColor.x >= 0.0) ? voxColor : mat.color;\n"
"\n"
"        vec3 pt = orig + dir * t;\n"
"\n"
"        // ── Phong lighting with shadow rays ──────────────────────\n"
"        float diff_i = 0.0, spec_i = 0.0;\n"
"        for (int l = 0; l < uLightCount; l++) {\n"
"            vec3  ldir  = normalize(uLight[l].xyz - pt);\n"
"            float ln    = dot(ldir, N);\n"
"\n"
"            // Shadow ray (toggleable)\n"
"            if (feat_shadows()) {\n"
"                vec3 sOrig = (dot(ldir, N) < 0.0)\n"
"                             ? pt - N * 0.001 : pt + N * 0.001;\n"
"                float st; vec3 sN; Material sM; vec3 sVC;\n"
"                if (scene_hit(sOrig, ldir, st, sN, sM, sVC) &&\n"
"                    st < length(uLight[l].xyz - pt)) continue;\n"
"            }\n"
"\n"
"            diff_i += uLight[l].w * max(0.0, ln);\n"
"            spec_i += uLight[l].w *\n"
"                pow(max(0.0, dot(reflect(-ldir, N), -dir)), mat.spec_exp);\n"
"        }\n"
"\n"
"        vec3 diffuse  = diffuse_color * diff_i;\n"
"        vec3 specular = vec3(spec_i);\n"
"\n"
"        color += weight * (diffuse * mat.albedo.x + specular * mat.albedo.y);\n"
"\n"
"        // ── Refraction (Snell's law) ────────────────────────────\n"
"        if (feat_refract() && mat.albedo.w > 0.0) {\n"
"            float eta = (dot(dir, N) < 0.0)\n"
"                        ? (1.0 / mat.ior) : mat.ior;\n"
"            vec3 faceN = (dot(dir, N) < 0.0) ? N : -N;\n"
"            vec3 rdir  = refract(dir, faceN, eta);\n"
"            if (length(rdir) > 0.001) {\n"
"                vec3 rOrig = (dot(rdir, N) < 0.0)\n"
"                             ? pt - N * 0.001 : pt + N * 0.001;\n"
"                float rt; vec3 rN; Material rM; vec3 rVC;\n"
"                if (scene_hit(rOrig, rdir, rt, rN, rM, rVC)) {\n"
"                    vec3 rpt = rOrig + rdir * rt;\n"
"                    vec3 rdiff = (rVC.x >= 0.0) ? rVC : rM.color;\n"
"                    float rd = 0.0, rs = 0.0;\n"
"                    for (int l = 0; l < uLightCount; l++) {\n"
"                        vec3 rl = normalize(uLight[l].xyz - rpt);\n"
"                        if (feat_shadows()) {\n"
"                            vec3 srO = rpt + rN * 0.001;\n"
"                            float srt; vec3 srN; Material srM; vec3 srVC;\n"
"                            if (scene_hit(srO, rl, srt, srN, srM, srVC) &&\n"
"                                srt < length(uLight[l].xyz - rpt))\n"
"                                continue;\n"
"                        }\n"
"                        rd += uLight[l].w * max(0.0, dot(rl, rN));\n"
"                        rs += uLight[l].w *\n"
"                            pow(max(0.0, dot(reflect(-rl,rN),-rdir)),\n"
"                                rM.spec_exp);\n"
"                    }\n"
"                    vec3 rc = rdiff * rd * rM.albedo.x\n"
"                            + vec3(rs) * rM.albedo.y;\n"
"                    color += weight * mat.albedo.w * rc;\n"
"                } else {\n"
"                    if (feat_envmap())\n"
"                        color += weight * mat.albedo.w * envmap_sample(rdir);\n"
"                    else {\n"
"                        float sky = 0.5 * (rdir.y + 1.0);\n"
"                        color += weight * mat.albedo.w\n"
"                               * mix(BG_TOP, BG_BOT, sky * 0.3);\n"
"                    }\n"
"                }\n"
"            }\n"
"        }\n"
"\n"
"        // ── Reflection (iterative) ──────────────────────────────\n"
"        if (feat_reflect() && mat.albedo.z > 0.0) {\n"
"            weight *= mat.albedo.z;\n"
"            dir  = reflect(dir, N);\n"
"            // Offset origin along or against normal depending on\n"
"            // which side the reflected ray exits.  Same as CPU:\n"
"            // dot(reflect_dir, N) < 0 means ray goes inward.\n"
"            orig = (dot(dir, N) < 0.0)\n"
"                   ? pt - N * 0.001\n"
"                   : pt + N * 0.001;\n"
"        } else {\n"
"            exhausted = false;\n"
"            break;\n"
"        }\n"
"    }\n"
"    // If the loop exhausted all bounces (e.g. inside a mirror sphere),\n"
"    // the CPU returns sky at the next recursive depth.  Match that by\n"
"    // adding the remaining weight as sky/envmap color.\n"
"    if (exhausted && weight > 0.001) {\n"
"        if (feat_envmap())\n"
"            color += weight * envmap_sample(dir);\n"
"        else {\n"
"            float sky = 0.5 * (dir.y + 1.0);\n"
"            color += weight * mix(BG_TOP, BG_BOT, sky * 0.3);\n"
"        }\n"
"    }\n"
"    return color;\n"
"}\n";

#endif /* GAME_GPU_SCENE_GLSL_H */
