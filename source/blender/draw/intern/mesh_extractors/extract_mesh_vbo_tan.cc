/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <climits>

#include "BLI_string.h"

#include "GPU_attribute_convert.hh"

#include "BKE_editmesh_tangent.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static void extract_tan_init_common(const MeshRenderData &mr,
                                    const MeshBatchCache &cache,
                                    GPUVertFormat *format,
                                    gpu::VertAttrType gpu_attr_type,
                                    CustomData *r_loop_data,
                                    int *r_v_len,
                                    int *r_tan_len,
                                    char r_tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME],
                                    bool *r_use_orco_tan)
{
  GPU_vertformat_deinterleave(format);

  const CustomData *cd_ldata = (mr.extract_type == MeshExtractType::BMesh) ? &mr.bm->ldata :
                                                                             &mr.mesh->corner_data;
  const CustomData *cd_vdata = (mr.extract_type == MeshExtractType::BMesh) ? &mr.bm->vdata :
                                                                             &mr.mesh->vert_data;
  uint32_t tan_layers = cache.cd_used.tan;
  const float3 *orco_ptr = static_cast<const float3 *>(CustomData_get_layer(cd_vdata, CD_ORCO));
  Span<float3> orco = orco_ptr ? Span(orco_ptr, mr.verts_num) : Span<float3>();
  Array<float3> orco_allocated;
  bool use_orco_tan = cache.cd_used.tan_orco != 0;

  int tan_len = 0;

  /* FIXME(#91838): This is to avoid a crash when orco tangent was requested but there are valid
   * uv layers. It would be better to fix the root cause. */
  if (tan_layers == 0 && use_orco_tan &&
      CustomData_get_layer_index(cd_ldata, CD_PROP_FLOAT2) != -1)
  {
    tan_layers = 1;
    use_orco_tan = false;
  }

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (tan_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_PROP_FLOAT2, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* Tangent layer name. */
      SNPRINTF(attr_name, "t%s", attr_safe_name);
      GPU_vertformat_attr_add(format, attr_name, gpu_attr_type);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2)) {
        GPU_vertformat_alias_add(format, "t");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2)) {
        GPU_vertformat_alias_add(format, "at");
      }

      STRNCPY(r_tangent_names[tan_len++], layer_name);
    }
  }
  if (use_orco_tan && orco.is_empty()) {
    /* If `orco` is not available compute it ourselves */
    orco_allocated.reinitialize(mr.verts_num);

    if (mr.extract_type == MeshExtractType::BMesh) {
      BMesh *bm = mr.bm;
      for (int v = 0; v < mr.verts_num; v++) {
        const BMVert *eve = BM_vert_at_index(bm, v);
        /* Exceptional case where #bm_vert_co_get can be avoided, as we want the original coords.
         * not the distorted ones. */
        orco_allocated[v] = eve->co;
      }
    }
    else {
      for (int v = 0; v < mr.verts_num; v++) {
        orco_allocated[v] = mr.vert_positions[v];
      }
    }
    /* TODO: This is not thread-safe. Draw extraction should not modify the mesh. */
    BKE_mesh_orco_verts_transform(const_cast<Mesh *>(mr.mesh), orco_allocated, false);
    orco = orco_allocated;
  }

  /* Start Fresh */
  CustomData_reset(r_loop_data);
  if (tan_len != 0 || use_orco_tan) {
    short tangent_mask = 0;
    bool calc_active_tangent = false;
    if (mr.extract_type == MeshExtractType::BMesh) {
      BKE_editmesh_loop_tangent_calc(mr.edit_bmesh,
                                     calc_active_tangent,
                                     r_tangent_names,
                                     tan_len,
                                     mr.bm_face_normals,
                                     mr.bm_loop_normals,
                                     orco,
                                     r_loop_data,
                                     mr.corners_num,
                                     &tangent_mask);
    }
    else {
      BKE_mesh_calc_loop_tangent_ex(mr.vert_positions,
                                    mr.faces,
                                    mr.corner_verts,
                                    mr.mesh->corner_tris(),
                                    mr.mesh->corner_tri_faces(),
                                    mr.sharp_faces,
                                    cd_ldata,
                                    calc_active_tangent,
                                    r_tangent_names,
                                    tan_len,
                                    mr.mesh->vert_normals(),
                                    mr.face_normals,
                                    mr.corner_normals,
                                    orco,
                                    r_loop_data,
                                    mr.corner_verts.size(),
                                    &tangent_mask);
    }
  }

  if (use_orco_tan) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    const char *layer_name = CustomData_get_layer_name(r_loop_data, CD_TANGENT, 0);
    GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    SNPRINTF(attr_name, "t%s", attr_safe_name);
    GPU_vertformat_attr_add(format, attr_name, gpu_attr_type);
    GPU_vertformat_alias_add(format, "t");
    GPU_vertformat_alias_add(format, "at");
  }

  int v_len = mr.corners_num;
  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", blender::gpu::VertAttrType::SFLOAT_32);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  *r_use_orco_tan = use_orco_tan;
  *r_v_len = v_len;
  *r_tan_len = tan_len;
}

gpu::VertBufPtr extract_tangents(const MeshRenderData &mr,
                                 const MeshBatchCache &cache,
                                 const bool use_hq)
{
  gpu::VertAttrType gpu_attr_type = use_hq ? gpu::VertAttrType::SNORM_16_16_16_16 :
                                             gpu::VertAttrType::SNORM_10_10_10_2;

  GPUVertFormat format = {0};
  CustomData corner_data;
  int v_len = 0;
  int tan_len = 0;
  bool use_orco_tan;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];
  extract_tan_init_common(mr,
                          cache,
                          &format,
                          gpu_attr_type,
                          &corner_data,
                          &v_len,
                          &tan_len,
                          tangent_names,
                          &use_orco_tan);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, v_len);

  if (use_hq) {
    short4 *tan_data = vbo->data<short4>().data();
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_named(
          &corner_data, CD_TANGENT, name);
      for (int corner = 0; corner < mr.corners_num; corner++) {
        *tan_data = gpu::convert_normal<short4>(layer_data[corner]);
        (*tan_data)[3] = (layer_data[corner][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_n(
          &corner_data, CD_TANGENT, 0);
      for (int corner = 0; corner < mr.corners_num; corner++) {
        *tan_data = gpu::convert_normal<short4>(layer_data[corner]);
        (*tan_data)[3] = (layer_data[corner][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
  }
  else {
    gpu::PackedNormal *tan_data = vbo->data<gpu::PackedNormal>().data();
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_named(
          &corner_data, CD_TANGENT, name);
      for (int corner = 0; corner < mr.corners_num; corner++) {
        *tan_data = gpu::convert_normal<gpu::PackedNormal>(layer_data[corner]);
        tan_data->w = (layer_data[corner][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_n(
          &corner_data, CD_TANGENT, 0);
      for (int corner = 0; corner < mr.corners_num; corner++) {
        *tan_data = gpu::convert_normal<gpu::PackedNormal>(layer_data[corner]);
        tan_data->w = (layer_data[corner][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
  }

  CustomData_free(&corner_data);
  return vbo;
}

static const GPUVertFormat &get_coarse_tan_format()
{
  static GPUVertFormat format = GPU_vertformat_from_attribute(
      "tan", gpu::VertAttrType::SFLOAT_32_32_32_32);
  return format;
}

gpu::VertBufPtr extract_tangents_subdiv(const MeshRenderData &mr,
                                        const DRWSubdivCache &subdiv_cache,
                                        const MeshBatchCache &cache)
{
  gpu::VertAttrType gpu_attr_type = gpu::VertAttrType::SFLOAT_32_32_32_32;
  GPUVertFormat format = {0};
  CustomData corner_data;
  int coarse_len = 0;
  int tan_len = 0;
  bool use_orco_tan;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];
  extract_tan_init_common(mr,
                          cache,
                          &format,
                          gpu_attr_type,
                          &corner_data,
                          &coarse_len,
                          &tan_len,
                          tangent_names,
                          &use_orco_tan);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, subdiv_cache.num_subdiv_loops));

  gpu::VertBuf *coarse_vbo = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(*coarse_vbo, get_coarse_tan_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*coarse_vbo, coarse_len);

  /* Index of the tangent layer in the compact buffer. Used layers are stored in a single buffer.
   */
  int pack_layer_index = 0;
  for (int i = 0; i < tan_len; i++) {
    float4 *tan_data = coarse_vbo->data<float4>().data();
    const char *name = tangent_names[i];
    const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_named(
        &corner_data, CD_TANGENT, name);
    for (int corner = 0; corner < mr.corners_num; corner++) {
      *tan_data = layer_data[corner];
      (*tan_data)[3] = (layer_data[corner][3] > 0.0f) ? 1.0f : -1.0f;
      tan_data++;
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(coarse_vbo);
    /* Include stride in offset. */
    const int dst_offset = int(subdiv_cache.num_subdiv_loops) * 4 * pack_layer_index++;
    draw_subdiv_interp_custom_data(subdiv_cache, *coarse_vbo, *vbo, GPU_COMP_F32, 4, dst_offset);
  }
  if (use_orco_tan) {
    float4 *tan_data = coarse_vbo->data<float4>().data();
    const float(*layer_data)[4] = (const float(*)[4])CustomData_get_layer_n(
        &corner_data, CD_TANGENT, 0);
    for (int corner = 0; corner < mr.corners_num; corner++) {
      *tan_data = layer_data[corner];
      (*tan_data)[3] = (layer_data[corner][3] > 0.0f) ? 1.0f : -1.0f;
      tan_data++;
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(coarse_vbo);
    /* Include stride in offset. */
    const int dst_offset = int(subdiv_cache.num_subdiv_loops) * 4 * pack_layer_index++;
    draw_subdiv_interp_custom_data(subdiv_cache, *coarse_vbo, *vbo, GPU_COMP_F32, 4, dst_offset);
  }

  CustomData_free(&corner_data);
  GPU_vertbuf_discard(coarse_vbo);
  return vbo;
}

}  // namespace blender::draw
