/*=========================================================================

 Program: FEMUS
 Module: MED_IO
 Authors: Sureka Pathmanathan, Giorgio Bornia

 Copyright (c) FEMTTU
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

//C++ include
#include <cassert>
#include <cstdio>
#include <fstream>
#include <tuple>


//local include
#include "MED_IO.hpp"
#include "Mesh.hpp"
#include "GeomElemQuad1.hpp"
#include "GeomElemQuad4.hpp"
#include "GeomElemQuad9.hpp"
#include "GeomElemHex1.hpp"
#include "GeomElemHex8.hpp"
#include "GeomElemHex27.hpp"
#include "GeomElemTri1.hpp"
#include "GeomElemTri3.hpp"
#include "GeomElemTri6.hpp"
#include "GeomElemTet1.hpp"
#include "GeomElemTet4.hpp"
#include "GeomElemTet10.hpp"
#include "GeomElemEdge1.hpp"
#include "GeomElemEdge2.hpp"
#include "GeomElemEdge3.hpp"



namespace femus
{
    

  const std::string MED_IO::mesh_ensemble  = "ENS_MAA";
  const std::string MED_IO::aux_zeroone    = "-0000000000000000001-0000000000000000001";
  const std::string MED_IO::elem_list      = "MAI";
  const std::string MED_IO::group_fam      = "FAM";
  const std::string MED_IO::connectivity   = "NOD";
  const std::string MED_IO::dofobj_indices = "NUM";
  const std::string MED_IO::node_list      = "NOE";
  const std::string MED_IO::coord_list     = "COO";
  const std::string MED_IO::group_ensemble = "FAS";
  const std::string MED_IO::group_elements = "ELEME";
  const std::string MED_IO::group_nodes    = "NOEUD";
  const uint MED_IO::max_length = 100;  ///@todo this length of the menu string is conservative enough...


  //How to determine a general connectivity:
  //you have to align the element with respect to the x-y-z (or xi-eta-zeta) reference frame,
  //and then look at the order in the med file.
  //For every node there is a location, and you have to put that index in that x-y-z location.
  //Look NOT at the NUMBERING, but at the ORDER!

// SALOME HEX27
//         1------17-------5
//        /|              /|
//       / |             / |
//      8  |   21      12  |
//     /   9      22   /   13
//    /    |          /    |
//   0------16-------4     |
//   | 20  |   26    |  25 |
//   |     2------18-|-----6       zeta
//   |    /          |    /          ^
//  11   /  24       15  /           |   eta
//   | 10      23    |  14           |  /
//   | /             | /             | /
//   |/              |/              |/
//   3-------19------7               -------> xi

// TEMPLATE HEX27  (for future uses)
//         X------X--------X
//        /|              /|
//       / |             / |
//      X  |   X        X  |
//     /   X      X    /   X
//    /    |          /    |
//   X------X--------X     |
//   | X   |   X     |  X  |
//   |     X------X--|-----X      zeta
//   |    /          |    /         ^
//   X   /  X        X   /          |   eta
//   |  X      X     |  X           |  /
//   | /             | /            | /
//   |/              |/             |/
//   X-------X-------X              -------> xi


  const unsigned MED_IO::SalomeToFemusVertexIndex[N_GEOM_ELS][MAX_EL_N_NODES] = {

    {4, 7, 3, 0, 5, 6, 2, 1, 15, 19, 11, 16, 13, 18, 9, 17, 12, 14, 10, 8, 23, 25, 22, 24, 20, 21, 26}, //HEX27
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, //TET10

    {
      3, 11, 5, 9, 10, 4,
      12, 17, 14, 15, 16, 13,
      0, 8, 2, 6, 7, 1
    },  //WEDGE18

    {0, 1, 2, 3, 4, 5, 6, 7, 8}, //QUAD9
    {0, 1, 2, 3, 4, 5},  //TRI6
    {0, 1, 2}            //EDGE3
  };


  const unsigned MED_IO::SalomeToFemusFaceIndex[N_GEOM_ELS][MAX_EL_N_FACES] = {
    {0, 4, 2, 5, 3, 1},
    {0, 1, 2, 3},
    {2, 1, 0, 4, 3},
    {0, 1, 2, 3},
    {0, 1, 2},
    {0, 1}
  };
 


  /// @todo extend to Wegdes (aka Prisms)
  /// @todo why pass coords other than get it through the Mesh class pointer?
  void MED_IO::read(const std::string& name, vector < vector < double> >& coords, const double Lref, std::vector<bool>& type_elem_flag) {

    Mesh& mesh = GetMesh();
    mesh.SetLevel(0);

    hid_t  file_id = H5Fopen(name.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);

    const std::vector<std::string> mesh_menus = get_mesh_names(file_id);

    if (mesh_menus.size() > 1) { std::cout << "Review the code because there is only one MultilevelMesh object and most likely things don't work" << std::endl; abort(); }
        
// dimension and geom_el types ===============

      const std::vector< std::tuple<std::string,unsigned int> > el_fe_type_per_dimension = set_mesh_dimension_and_get_geom_elems_by_looping_over_element_types(file_id, mesh_menus[0]);
      
// meshes ========================
     for(unsigned j = 0; j < mesh_menus.size(); j++) {

// node coordinates
            set_node_coordinates(file_id, mesh_menus[j], coords, Lref);
    
// Groups of the mesh ===============
     std::vector< GroupInfo >     group_info = get_group_vector_flags_per_mesh(file_id,mesh_menus[j]);
    
          for(unsigned i = 0; i < group_info.size(); i++) {
              compute_group_geom_elem_and_size(file_id, mesh_menus[j],group_info[i]);
          }
          
    
// dimension loop
      for(unsigned i = 0; i < mesh.GetDimension(); i++) {

            set_elem_connectivity(file_id, mesh_menus[j], i,             el_fe_type_per_dimension[i], type_elem_flag);  //type_elem_flag is to say "There exists at least one element of that type in the mesh"

         set_elem_group_ownership(file_id, mesh_menus[j], i, std::get<0>(el_fe_type_per_dimension[i]), group_info);
      
        get_global_elem_numbering(file_id, mesh_menus[j],    std::get<0>(el_fe_type_per_dimension[i]));
        
      }
             
    
    find_boundary_faces_and_set_face_flags(file_id,group_info);
    
    }
    
    

    H5Fclose(file_id);

  }




/// @todo do we need these numbers for us?
   void MED_IO::get_global_elem_numbering(const hid_t&  file_id, const std::string mesh_menu, const std::string el_fe_type_per_dimension) const  {
       
         //NUM ***************************
       hsize_t dims_num[2];
       std::string my_mesh_name_dir = mesh_ensemble +  "/" + mesh_menu + "/" +  aux_zeroone + "/" + elem_list + "/";  ///@todo here we have to loop
        std::string node_name_dir_i = my_mesh_name_dir + el_fe_type_per_dimension + "/" + dofobj_indices;
        hid_t dtset_num = H5Dopen(file_id, node_name_dir_i.c_str(), H5P_DEFAULT);
        hid_t filespace_num = H5Dget_space(dtset_num);
        hid_t status_bdry  = H5Sget_simple_extent_dims(filespace_num, dims_num, NULL);
        if(status_bdry == 0) {    std::cerr << "MED_IO::read dims not found";  abort();  }
        H5Dclose(dtset_num); 
        
   }
   
   
     //here I need a routine to compute the group GeomElem and the group size

    //separate groups by dimension
    //as soon as an entry is equal to the group_salome_flag, that means the dimension is that of the current element dataset
   void MED_IO::compute_group_geom_elem_and_size(const hid_t&  file_id, const std::string mesh_menu, GroupInfo & group_info) const {
       
      std::string my_mesh_name_dir = mesh_ensemble +  "/" + mesh_menu + "/" +  aux_zeroone + "/" + elem_list + "/";  ///@todo here we have to loop

      hsize_t     n_geom_el_types;
      hid_t       gid = H5Gopen(file_id, my_mesh_name_dir.c_str(), H5P_DEFAULT);
      hid_t status = H5Gget_num_objs(gid, &n_geom_el_types);
        
    std::vector<char*> elem_types(n_geom_el_types);

    bool group_found = false;
    
    //loop over all FAM fields until the group is found
    unsigned j = 0;
    while (j < elem_types.size() && group_found == false) {
        
      hsize_t dims_fam[2];
      elem_types[j] = new char[max_length];
      H5Gget_objname_by_idx(gid, j, elem_types[j], max_length); ///@deprecated see the HDF doc to replace this
      std::string elem_types_str(elem_types[j]);
      
       std::string fam_name_dir_i = my_mesh_name_dir + elem_types_str + "/" + group_fam;
       hid_t dtset_fam = H5Dopen(file_id, fam_name_dir_i.c_str(), H5P_DEFAULT);
       hid_t filespace_fam = H5Dget_space(dtset_fam);
       hid_t status_fam  = H5Sget_simple_extent_dims(filespace_fam, dims_fam, NULL);
       if(status_fam == 0) {     std::cerr << "MED_IO::read dims not found";  abort();  }
      
        const unsigned n_elements = dims_fam[0];
        std::vector<int> fam_map(n_elements);
        hid_t status_conn = H5Dread(dtset_fam, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, fam_map.data() );

        int group_size = 0;
        for(unsigned k = 0; k < fam_map.size(); k++) {

            if ( fam_map[k] == group_info._salome_flag )   {
                group_found = true;
                group_size++;
                std::cout << "Current flag " << fam_map[k] << " matches " << group_info._salome_flag << " " << elem_types[j] << std::endl; 
                group_info._geom_el = get_geom_elem_from_med_name(elem_types_str);
            }
        
       }
       group_info._size = group_size;
    
        H5Dclose(dtset_fam);
        
        j++;
     }
          
       H5Gclose(gid);
       
        return;
        
    }

    
    
   void MED_IO::find_boundary_faces_and_set_face_flags(const hid_t&  file_id,const std::vector<GroupInfo> & group_info)  {
       
       Mesh& mesh = GetMesh();
       
       //after the volume connectivity has been read, I can loop over all boundary groups
       // right now, I'll loop over all groups whose geometric element has dimension n-1
       // later, one may have have other groups of dimension n-1 that are not on the boundary
        for(unsigned iel = 0; iel < mesh.GetNumberOfElements(); iel++) {
                   unsigned iel_type = mesh.GetElementType(iel);
            for(unsigned f = 0; f < mesh.GetElementFaceNumber(iel); f++) {
                
                

               for(unsigned gv = 0; gv < group_info.size(); gv++) {
                               if ( group_info[gv]._geom_el->get_dimension() == mesh.GetDimension() -1   ) { //boundary groups
      
                                   
                                   
                   
                               }
                     }
                 } //faces
               }// end volume elements
   }
   
   
  
  //  we loop over all elements and see which ones are of that group
   void MED_IO::set_elem_group_ownership(const hid_t&  file_id, const std::string mesh_menu,  const int i, const std::string el_fe_type_per_dimension, const std::vector<GroupInfo> & group_info)  {
       
       Mesh& mesh = GetMesh();
       
       //FAM ***************************
        hsize_t dims_fam[2];
       std::string my_mesh_name_dir = mesh_ensemble +  "/" + mesh_menu + "/" +  aux_zeroone + "/" + elem_list + "/";  ///@todo here we have to loop
       std::string fam_name_dir_i = my_mesh_name_dir + el_fe_type_per_dimension + "/" + group_fam;
        hid_t dtset_fam = H5Dopen(file_id, fam_name_dir_i.c_str(), H5P_DEFAULT);
        hid_t filespace_fam = H5Dget_space(dtset_fam);
        hid_t status_fam  = H5Sget_simple_extent_dims(filespace_fam, dims_fam, NULL);
        if(status_fam == 0) {     std::cerr << "MED_IO::read dims not found";  abort();  }
        
        const unsigned n_elements = dims_fam[0];
        std::vector<int> fam_map(n_elements);
        hid_t status_conn = H5Dread(dtset_fam, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, fam_map.data());

        
// ****************** Volume *******************************************    
           if ( i == (mesh.GetDimension() - 1 ) ) { //volume
    std::vector < unsigned > materialElementCounter(3,0);  //I think this counts who is fluid, who is solid, who whatever else, need to double check with the Gambit input files
    const unsigned group_property_fluid_probably          = 2;
    const unsigned group_property_something_else_probably = 3;
    const unsigned group_property_solid_probably          = 4;

    //I have to split the groups with the dimensions
    //I have to compute the number of elements of each group
    
        for(unsigned gv = 0; gv < group_info.size(); gv++) {
            if ( i == group_info[gv]._geom_el->get_dimension() - 1 ) {
        for(unsigned g = 0; g < fam_map.size()/*group_info[gv]._size*//*number_of_group_elements*/; g++) {
            if (fam_map[g] == group_info[gv]._salome_flag)   mesh.el->SetElementGroup(g, /*fam_map[g]*/ group_info[gv]._user_defined_flag /*gr_integer_name*/);  //I think that 1 is set later as the default  group number
//         mesh.el->SetElementMaterial(elem_indices[g] - 1 - n_elements_b_bb, group_info[gv]._user_defined_property /*gr_material*/);
// 	
         if(group_info[gv]._user_defined_property/*gr_material*/ == group_property_fluid_probably          ) materialElementCounter[0] += 1;
	else if(group_info[gv]._user_defined_property/*gr_material*/ == group_property_something_else_probably ) materialElementCounter[1] += 1;
	else                                                            materialElementCounter[2] += 1;

                }
            }  //groups of the current dimension
        }  //loop over all groups
           
    mesh.el->SetElementGroupNumber(1/*n_groups_of_that_space_dimension*/);
    mesh.el->SetMaterialElementCounter(materialElementCounter);
           }
// ****************** Volume, end *******************************************    
        

// ****************** Boundary *******************************************    
        else   if ( i == (mesh.GetDimension() - 1 - 1) ) { //boundary
                    
    //loop over volume elements
    //extract faces

//   // read boundary **************** D
  for (unsigned k=0; k < group_info.size()/*nbcd*//*@todo these should be the groups that are "boundary groups"*/; k++) { //
           int value = group_info[k]._user_defined_flag;       //flag of the boundary portion
      unsigned nface = group_info[k]._size;  //number of elements in a portion of boundary
               value = - (value + 1);  ///@todo these boundary indices need to be NEGATIVE,  so the value in salome must be POSITIVE
    for (unsigned f = 0; f < nface; f++) {
//       unsigned iel =,   //volume element to which the face belongs
//       iel--;
//       unsigned iface; //index of the face in that volume element
//       iface = MED_IO::SalomeToFemusFaceIndex[mesh.el->GetElementType(iel)][iface-1u];
//       mesh.el->SetFaceElementIndex(iel,iface,value);  //value is (-1) for element faces that are not boundary faces
    }
  }
//   // end read boundary **************** D
                    
        } //end (volume pos - 1)
// ****************** Boundary end *******************************************    
    
        H5Dclose(dtset_fam);

   } 

   // Connectivities in MED files are stored on a per-node basis: first all 1st nodes, then all 2nd nodes, and so on.
   // Instead, in Gambit they are stored on a per-element basis
   void MED_IO::set_elem_connectivity(const hid_t&  file_id, const std::string mesh_menu, const unsigned i, const std::tuple<std::string,unsigned int>  & el_fe_type_per_dimension, std::vector<bool>& type_elem_flag) {

       Mesh& mesh = GetMesh();
       
       const unsigned el_nodes_per_dimension = std::get<1>(el_fe_type_per_dimension);

       std::string my_mesh_name_dir = mesh_ensemble +  "/" + mesh_menu + "/" +  aux_zeroone + "/" + elem_list + "/";  ///@todo here we have to loop
       hsize_t dims_i[2];
        // NOD ***************************
        std::string conn_name_dir_i = my_mesh_name_dir +  std::get<0>(el_fe_type_per_dimension) + "/" + connectivity;
        hid_t dtset_conn = H5Dopen(file_id, conn_name_dir_i.c_str(), H5P_DEFAULT);
        hid_t filespace = H5Dget_space(dtset_conn);
        hid_t status_els_i  = H5Sget_simple_extent_dims(filespace, dims_i, NULL);
        if(status_els_i == 0) { std::cerr << "MED_IO::read dims not found";   abort();   }
        
            
      const int dim_conn = dims_i[0];
      const unsigned n_elems_per_dimension = dim_conn / el_nodes_per_dimension;
      std::cout << " Number of elements of dimension " << (i+1) << " in med file: " <<  n_elems_per_dimension <<  std::endl;

      // SET NUMBER OF VOLUME ELEMENTS
        if ( i == mesh.GetDimension() - 1 ) { 
      mesh.SetNumberOfElements(n_elems_per_dimension);
      mesh.el = new elem(n_elems_per_dimension);    ///@todo check where this is going to be deleted

      // READ CONNECTIVITY MAP
      int* conn_map = new  int[dim_conn];
      hid_t status_conn = H5Dread(dtset_conn, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, conn_map);
      if(status_conn != 0) {     std::cout << "MED_IO::read: connectivity not found";   abort();   }
      
      
            for(unsigned iel = 0; iel < n_elems_per_dimension; iel++) {
        mesh.el->SetElementGroup(iel, 1);
        unsigned nve = el_nodes_per_dimension;  /// @todo this is only one element type
        if(nve == 27) {
          type_elem_flag[0] = type_elem_flag[3] = true;
          mesh.el->AddToElementNumber(1, "Hex");
          mesh.el->SetElementType(iel, HEX);
        }
        else if(nve == 10) {
          type_elem_flag[1] = type_elem_flag[4] = true;
          mesh.el->AddToElementNumber(1, "Tet");
          mesh.el->SetElementType(iel, TET);
        }
        else if(nve == 18) {
          type_elem_flag[2] = type_elem_flag[3] = type_elem_flag[4] = true;
          mesh.el->AddToElementNumber(1, "Wedge");
          mesh.el->SetElementType(iel, WEDGE);
        }
        else if(nve == 9) {
          type_elem_flag[3] = true;
          mesh.el->AddToElementNumber(1, "Quad");
          mesh.el->SetElementType(iel, QUAD);
        }
        else if(nve == 6 && mesh.GetDimension() == 2) {
          type_elem_flag[4] = true;
          mesh.el->AddToElementNumber(1, "Triangle");
          mesh.el->SetElementType(iel, TRI);
        }
        else if(nve == 3 && mesh.GetDimension() == 1) {
          mesh.el->AddToElementNumber(1, "Line");
          mesh.el->SetElementType(iel, LINE);
        }
        else {
          std::cout << "Error! Invalid element type in reading  File!" << std::endl;
          std::cout << "Error! Use a second order discretization" << std::endl;
          abort();
        }
        for(unsigned i = 0; i < nve; i++) {
          unsigned inode = SalomeToFemusVertexIndex[mesh.el->GetElementType(iel)][i];
          mesh.el->SetElementDofIndex(iel, inode, conn_map[iel + i * n_elems_per_dimension ] - 1u);  //MED connectivity is stored on a per-node basis, not a per-element basis
         }
       }
      
           // clean
      delete [] conn_map;

     }
        
      H5Dclose(dtset_conn);

}
      
      
      
      
   void MED_IO::set_node_coordinates(const hid_t&  file_id, const std::string mesh_menu, vector < vector < double> > & coords, const double Lref) {
       
       Mesh& mesh = GetMesh();
       hsize_t dims[2];
       
      std::string coord_dataset = mesh_ensemble +  "/" + mesh_menu + "/" +  aux_zeroone + "/" + node_list + "/" + coord_list + "/";  ///@todo here we have to loop

      hid_t dtset = H5Dopen(file_id, coord_dataset.c_str(), H5P_DEFAULT);

      // SET NUMBER OF NODES
      hid_t filespace = H5Dget_space(dtset);    /* Get filespace handle first. */
      hid_t status_dims  = H5Sget_simple_extent_dims(filespace, dims, NULL);
      if(status_dims == 0) std::cerr << "MED_IO::read dims not found";
      // reading xyz_med
      unsigned int n_nodes = dims[0] / 3; //mesh.GetDimension();
      double*   xyz_med = new double[dims[0]];
      std::cout << " Number of nodes in med file " <<  n_nodes << " " <<  std::endl;

      mesh.SetNumberOfNodes(n_nodes);

      // SET NODE COORDINATES
      coords[0].resize(n_nodes);
      coords[1].resize(n_nodes);
      coords[2].resize(n_nodes);

      H5Dread(dtset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, xyz_med);
      H5Dclose(dtset);

      if(mesh.GetDimension() == 3) {
        for(unsigned j = 0; j < n_nodes; j++) {
          coords[0][j] = xyz_med[j] / Lref;
          coords[1][j] = xyz_med[j + n_nodes] / Lref;
          coords[2][j] = xyz_med[j + 2 * n_nodes] / Lref;
        }
      }

      else if(mesh.GetDimension() == 2) {
        for(unsigned j = 0; j < n_nodes; j++) {
          coords[0][j] = xyz_med[j] / Lref;
          coords[1][j] = xyz_med[j + n_nodes] / Lref;
          coords[2][j] = 0.;
        }
      }

      else if(mesh.GetDimension() == 1) {
        for(unsigned j = 0; j < n_nodes; j++) {
          coords[0][j] = xyz_med[j] / Lref;
          coords[1][j] = 0.;
          coords[2][j] = 0.;
        }
      }

      delete[] xyz_med;

      //   // end read NODAL COORDINATES ************* C
   }
  
  
  
  //salome family; our name; our property; group size 
  /// @todo check the underscores according to our naming standard
  const GroupInfo  MED_IO::get_group_flags_per_mesh(const std::string & group_name) const {
  
      GroupInfo  group_info;

      const int str_pos = 0;
      std::pair<int,int> gr_family_in_salome_pair = isolate_number_in_string( group_name, str_pos );
      std::pair<int,int> gr_name_pair             = isolate_number_in_string( group_name, gr_family_in_salome_pair.second + 1 );
      std::pair<int,int> gr_property_pair         = isolate_number_in_string( group_name, gr_name_pair.second + 1 );
      group_info._salome_flag           = gr_family_in_salome_pair.first;
      group_info._user_defined_flag     = gr_name_pair.first;
      group_info._user_defined_property = gr_property_pair.first;      
    
    return  group_info;
 }


     // ************** Groups of each Mesh *********************************
 const std::vector< GroupInfo > MED_IO::get_group_vector_flags_per_mesh(const hid_t&  file_id, const std::string & mesh_menu) const {
     
     std::string group_list = group_ensemble +  "/" + mesh_menu + "/" + group_elements;
     hid_t  gid_groups      = H5Gopen(file_id, group_list.c_str(), H5P_DEFAULT);
    if(gid_groups != 0) {
      std::cout << "Groups of Elements not found" << std::endl;
//       abort();
    }
     
     hsize_t n_groups = 0;
     hid_t status_groups = H5Gget_num_objs(gid_groups, &n_groups);
    
     std::vector< std::string >             group_names(n_groups);
     std::vector< GroupInfo >                group_info(n_groups);
        
    for(unsigned j = 0; j < n_groups; j++) {
         
        char*   group_names_char = new char[max_length];
        H5Gget_objname_by_idx(gid_groups, j, group_names_char, max_length); ///@deprecated see the HDF doc to replace this
              group_names[j] = group_names_char;
              delete[] group_names_char;
        
        group_info[j] = get_group_flags_per_mesh(group_names[j]);
              
    }
   
    return group_info;
    
  }
    
    
  // compute number of Mesh fields in Salome file ==============
  const std::vector<std::string> MED_IO::get_mesh_names(const hid_t&  file_id) const {
      
    hid_t  gid = H5Gopen(file_id, mesh_ensemble.c_str(), H5P_DEFAULT);
    
    hsize_t     n_meshes_ens;
    hid_t status = H5Gget_num_objs(gid, &n_meshes_ens); // number of meshes
    if(status != 0) {
      std::cout << "Number of mesh menus not found";
      abort();
    }
  
    std::vector<std::string>  mesh_menus;

    unsigned n_meshes = 0;

    for(unsigned j = 0; j < n_meshes_ens; j++) {

      char*   menu_names_j = new char[max_length];
      H5Gget_objname_by_idx(gid, j, menu_names_j, max_length); ///@deprecated see the HDF doc to replace this
      std::string tempj(menu_names_j);


      if(tempj.substr(0, 4).compare("Mesh") == 0) {
        n_meshes++;
        mesh_menus.push_back(tempj);
      }
      else { std::cout << "Mesh MED fields must start with the word Mesh" << std::endl; abort();
      }

    }
      if (n_meshes != n_meshes_ens) { std::cout << "Meshes are called with Mesh"; abort(); }

      H5Gclose(gid);
      
    return mesh_menus;
}
    
    
  // This function starts from a given point in a string,
  // finds the first two occurrences of underscores,
  // and gets the string in between them
  // If it finds only one underscore and it gets to end-of-file, I want to get that string there
  std::pair<int,int>  MED_IO::isolate_number_in_string(const std::string &  string_in, const int begin_pos_to_investigate) const {
      
    try {
        
      int str_pos = begin_pos_to_investigate;
      std::cout << "Start searching in string " << string_in << " from the character " << string_in.at(str_pos) << " in position " << str_pos <<  std::endl;

      std::vector<int> two_adj_underscores_pos(2,0);

      std::string temp_buffer; 

      assert(str_pos < string_in.size());   temp_buffer = string_in.at(str_pos);
      
      if ( temp_buffer.compare( "_" ) == 0 )  {  std::cout <<  "I don't want to start with an underscore" << std::endl; abort();  }
      
      //begin search for the 1st underscore -------------------------------
      while ( temp_buffer.compare( "_" ) != 0  &&  ( str_pos < (string_in.size() - 1) )  ) {  str_pos++; temp_buffer = string_in.at(str_pos); }
      
                    two_adj_underscores_pos[0] = str_pos;
      
      //begin search for the 2nd underscore -------------------------------
      if ( str_pos < (string_in.size() - 1) ) {
          str_pos++; temp_buffer = string_in.at(str_pos);
      while ( temp_buffer.compare( "_" ) != 0  &&  ( str_pos < (string_in.size() - 1) )  ) {  str_pos++; temp_buffer = string_in.at(str_pos); }
                    two_adj_underscores_pos[1] = str_pos;
  
      }
      else if ( str_pos == (string_in.size() - 1) ) {  //if it reaches the end, it does so after the 1st iteration, because there is an EVEN number of underscores
                    two_adj_underscores_pos[0] = begin_pos_to_investigate - 1; //this allows for numbers with more than 1 digit
                    two_adj_underscores_pos[1] = str_pos+1;
      }
      //end search for the 2 underscores -------------------------------
    
      std::pair<int,int>  delimiting_positions(two_adj_underscores_pos[0],two_adj_underscores_pos[1]);

            int string_to_extract_pos    = delimiting_positions.first + 1;
            int string_to_extract_length = delimiting_positions.second - delimiting_positions.first - 1;
      
      std::cout <<  string_to_extract_pos << " " << string_to_extract_length << " " << string_in.substr(string_to_extract_pos,string_to_extract_length).c_str() << " " << std::endl;    

      const int flag = atoi( string_in.substr(string_to_extract_pos,string_to_extract_length).c_str() );
      
      std::pair<int,int>  flag_and_flag_pos_in_string(flag,two_adj_underscores_pos[1]);
      
      return flag_and_flag_pos_in_string;
      
    }

    catch(const std::out_of_range& e) {
          std::cerr <<  "Reading out of range" << std::endl; abort(); 
    }

      
  }
  
  


 const std::vector<std::string> MED_IO::get_geom_elem_type_per_dimension(
    const hid_t & file_id,
    const std::string  my_mesh_name_dir
  ) 
    {

    Mesh& mesh = GetMesh();

    const int n_fem_types = mesh.GetDimension();
    
    std::cout << "No hybrid mesh for now: only 1 FE type per dimension" << std::endl;
    
    // Get the element name
    char** el_fem_type = new char*[n_fem_types];

    std::vector<int> index(n_fem_types);  //is this used?

    const uint fe_name_nchars = 4;

     std::vector<std::string> fe_type_per_dimension(mesh.GetDimension());
    
    for(int i = 0; i < (int) n_fem_types; i++) {

      el_fem_type[i] = new char[fe_name_nchars];
      H5Lget_name_by_idx(file_id, my_mesh_name_dir.c_str(), H5_INDEX_NAME, H5_ITER_INC, i, el_fem_type[i], fe_name_nchars, H5P_DEFAULT);
      std::string temp_i(el_fem_type[i]);

      if(mesh.GetDimension() == 3) {

        if( temp_i.compare("HE8") == 0 ||
            temp_i.compare("H20") == 0 ||
            temp_i.compare("H27") == 0 ||
            temp_i.compare("TE4") == 0 ||
            temp_i.compare("T10") == 0) {
          index[mesh.GetDimension() - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1] = el_fem_type[i];
        }
        else if(temp_i.compare("QU4") == 0 ||
                temp_i.compare("QU8") == 0 ||
                temp_i.compare("QU9") == 0 ||
                temp_i.compare("TR3") == 0 ||
                temp_i.compare("TR6") == 0) {
          index[mesh.GetDimension() - 1 - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1 - 1] = el_fem_type[i];
        }
        else if(temp_i.compare("SE2") == 0 ||
                temp_i.compare("SE3") == 0) {
          index[mesh.GetDimension() - 1 - 1 - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1 - 1 - 1] =  el_fem_type[i];
        }

      }

      else if(mesh.GetDimension() == 2) {

        if( temp_i.compare("QU4") == 0 ||
            temp_i.compare("QU8") == 0 ||
            temp_i.compare("QU9") == 0 ||
            temp_i.compare("TR3") == 0 ||
            temp_i.compare("TR6") == 0) {
          index[mesh.GetDimension() - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1] = el_fem_type[i];
        }
        else if(temp_i.compare("SE2") == 0 ||
                temp_i.compare("SE3") == 0) {
          index[mesh.GetDimension() - 1 - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1 - 1] = el_fem_type[i];
        }

      }

      else if(mesh.GetDimension() == 1) {
        if( temp_i.compare("SE2") == 0 ||
            temp_i.compare("SE3") == 0) {
          index[mesh.GetDimension() - 1] = i;
          fe_type_per_dimension[mesh.GetDimension() - 1] = el_fem_type[i];
        }
      }

    }

    // clean
    for(int i = 0; i < (int)n_fem_types; i++) delete[] el_fem_type[i];
    delete[] el_fem_type;

    return fe_type_per_dimension;
  }

// figures out the Mesh dimension by looping over element types
/// @todo this determination of the dimension from the mesh file would not work with a 2D mesh embedded in 3D

  std::vector< std::tuple<std::string,unsigned int> >  MED_IO::set_mesh_dimension_and_get_geom_elems_by_looping_over_element_types(const hid_t &  file_id, const std::string & mesh_menus)  {
      
      
      std::string my_mesh_name_dir = mesh_ensemble +  "/" + mesh_menus + "/" +  aux_zeroone + "/" + elem_list + "/";  ///@todo here we have to loop

      hsize_t     n_fem_type;
      hid_t       gid = H5Gopen(file_id, my_mesh_name_dir.c_str(), H5P_DEFAULT);
      hid_t status = H5Gget_num_objs(gid, &n_fem_type);
      if(status != 0) {   std::cout << "MED_IO::read_fem_type:   H5Gget_num_objs not found";  abort();   }

    Mesh& mesh = GetMesh();
    uint mydim = 1;  //this is the initial value, then it will be updated below
    mesh.SetDimension(mydim);


    std::vector<char*> elem_types(n_fem_type);
    
    
    for(unsigned j = 0; j < elem_types.size(); j++) {
      elem_types[j] = new char[max_length];
      H5Gget_objname_by_idx(gid, j, elem_types[j], max_length); ///@deprecated see the HDF doc to replace this
      std::string elem_types_str(elem_types[j]);

      if( elem_types_str.compare("HE8") == 0 ||
          elem_types_str.compare("H20") == 0 ||
          elem_types_str.compare("H27") == 0 ||
          elem_types_str.compare("TE4") == 0 ||
          elem_types_str.compare("T10") == 0)      {
        mydim = 3;
      }
      else if(elem_types_str.compare("QU4") == 0 ||
              elem_types_str.compare("QU8") == 0 ||
              elem_types_str.compare("QU9") == 0 ||
              elem_types_str.compare("TR3") == 0 ||
              elem_types_str.compare("TR6") == 0) {
        mydim = 2;
      }

      if(mydim > mesh.GetDimension()) mesh.SetDimension(mydim);

    }  //end for

      H5Gclose(gid);
      

      std::vector< std::string >  el_fe_type_per_dimension = get_geom_elem_type_per_dimension(file_id, my_mesh_name_dir);
      std::vector< unsigned int >   el_nodes_per_dimension(mesh.GetDimension(),0);
      std::vector< std::tuple<std::string,unsigned int> >  elem_tuple(mesh.GetDimension());
      
      for(unsigned i = 0; i < mesh.GetDimension(); i++) {
            el_nodes_per_dimension[i] = get_elem_number_of_nodes(el_fe_type_per_dimension[i]);
                        elem_tuple[i] = std::make_tuple(el_fe_type_per_dimension[i],el_nodes_per_dimension[i]);
      }
      
       if(mesh.GetDimension() != n_fem_type) { std::cout << "Mismatch between dimension and number of element types" << std::endl;   abort();  }
     
    return elem_tuple;
  }
  
  

  unsigned  MED_IO::get_elem_number_of_nodes(const  std::string el_type) const {

    unsigned Node_el;

         if(el_type.compare("HE8") == 0) Node_el = 8;
    else if(el_type.compare("H20") == 0) Node_el = 20;
    else if(el_type.compare("H27") == 0) Node_el = 27;

    else if(el_type.compare("TE4") == 0) Node_el = 4;
    else if(el_type.compare("T10") == 0) Node_el = 10;

    else if(el_type.compare("QU4") == 0) Node_el = 4;
    else if(el_type.compare("QU8") == 0) Node_el = 8;
    else if(el_type.compare("QU9") == 0) Node_el = 9;

    else if(el_type.compare("TR3") == 0) Node_el = 3;
    else if(el_type.compare("TR6") == 0) Node_el = 6;

    else if(el_type.compare("SE3") == 0) Node_el = 3;
    else if(el_type.compare("SE2") == 0) Node_el = 2;
    else {
      std::cout << "MED_IO::read: element not supported";
      abort();
    }

    return Node_el;
  }
  
  
  GeomElemBase * MED_IO::get_geom_elem_from_med_name(const  std::string el_type) const {
      
         if(el_type.compare("HE8") == 0) return new FEHex8();
    else if(el_type.compare("H20") == 0) abort(); ///@todo //return new FEHex20();
    else if(el_type.compare("H27") == 0) return new FEHex27();

    else if(el_type.compare("TE4") == 0) return new FETet4();
    else if(el_type.compare("T10") == 0) return new FETet10();

    else if(el_type.compare("QU4") == 0) return new FEQuad4();
    else if(el_type.compare("QU8") == 0) abort();  
    else if(el_type.compare("QU9") == 0) return new FEQuad9();

    else if(el_type.compare("TR3") == 0) return new FETri3();
    else if(el_type.compare("TR6") == 0) return new FETri6();
    else if(el_type.compare("TR7") == 0) abort(); 

    else if(el_type.compare("SE2") == 0) return new FEEdge2();
    else if(el_type.compare("SE3") == 0) return new FEEdge3();
    else {
      std::cout << "MED_IO::read: element not supported";
      abort();
    }
  
      
  }
  

} //end namespace femus

