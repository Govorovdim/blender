/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "IO_wavefront_obj.hh"
#include "obj_import_mtl.hh"
#include "obj_import_objects.hh"

struct Main;
struct Mesh;
struct Material;
struct Object;
struct OBJImportParams;

namespace blender::io::obj {

/**
 * Make a Blender Mesh Object from a Geometry of GEOM_MESH type.
 */
class MeshFromGeometry : NonMovable, NonCopyable {
 private:
  Geometry &mesh_geometry_;
  const GlobalVertices &global_vertices_;

 public:
  MeshFromGeometry(Geometry &mesh_geometry, const GlobalVertices &global_vertices)
      : mesh_geometry_(mesh_geometry), global_vertices_(global_vertices)
  {
  }

  Mesh *create_mesh(const OBJImportParams &import_params);

  Object *create_mesh_object(Main *bmain,
                             Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                             Map<std::string, Material *> &created_materials,
                             const OBJImportParams &import_params);

 private:
  /**
   * OBJ files coming from the wild might have faces that are invalid in Blender
   * (mostly with duplicate vertex indices, used by some software to indicate
   * faces with holes). This method tries to fix them up.
   */
  void fixup_invalid_faces();
  void create_vertices(Mesh *mesh);
  /**
   * Create faces for the Mesh, set smooth shading flags, Materials.
   */
  void create_faces(Mesh *mesh, bool use_vertex_groups);
  /**
   * Add explicitly imported OBJ edges to the mesh.
   */
  void create_edges(Mesh *mesh);
  /**
   * Add UV layer and vertices to the Mesh.
   */
  void create_uv_verts(Mesh *mesh);
  /**
   * Add materials and the node-tree to the Mesh Object.
   */
  void create_materials(Main *bmain,
                        Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                        Map<std::string, Material *> &created_materials,
                        Object *obj,
                        bool relative_paths,
                        eOBJMtlNameCollisionMode mtl_name_collision_mode);
  void create_normals(Mesh *mesh);
  void create_colors(Mesh *mesh);
  void create_vertex_groups(Object *obj);

  bool has_normals() const;
};

}  // namespace blender::io::obj
