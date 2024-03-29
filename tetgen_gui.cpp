#include <igl/readOBJ.h>
#include <igl/readOFF.h>
#include <igl/readPLY.h>
#include <igl/readSTL.h>
#include <igl/remove_unreferenced.h>
#include <igl/boundary_facets.h>
#include <igl/copyleft/tetgen/mesh_to_tetgenio.h>
#include <igl/copyleft/tetgen/tetgenio_to_tetmesh.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <imgui/imgui.h>
#include <tetgen/tetgen.h>
#include <fmt/ostream.h>

igl::opengl::glfw::Viewer viewer;

typedef Eigen::MatrixXd dMat;
typedef Eigen::MatrixXi iMat;
typedef Eigen::VectorXi iVec;
typedef Eigen::Matrix<bool, Eigen::Dynamic, 1> bVec;

// input visualization & representation
dMat V_ori;
iMat F_ori;

tetgenio tetio;

// output representation
int t_offset = 0; // when input is stl, t_offset should be 1
dMat V_tet;
iMat T_tet;
iMat F_tet;
iVec TX_tet;

// visualize representation
bVec mask;
bVec mask_prev;
iMat T_vis;
iMat TX_vis;
iMat F_vis;

void show_origin()
{
  viewer.data().clear();
  viewer.data().set_mesh(V_ori, F_ori);
  viewer.core().align_camera_center(V_ori);
}

void show_tet()
{
  viewer.data().clear();

  // update T_vis by mask
  bVec mask_T(TX_tet.size());
  for (int i = 0; i < TX_tet.size(); i++)
    mask_T(i) = mask(TX_tet(i));
  igl::slice_mask(T_tet, mask_T, 1, T_vis);
  igl::slice_mask(TX_tet, mask_T, 1, TX_vis);

  // show T_vis
  igl::boundary_facets(T_vis, F_vis);
  viewer.data().set_mesh(V_tet, F_vis);
}

bool load_tetgenio(const std::string filename)
{
  char* f_tmp = new char[filename.size() + 1];
  std::strcpy(f_tmp, filename.c_str());

  auto ends_with = [](const std::string long_str, const std::string suffix)
  {
    return long_str.substr(long_str.size() - suffix.size(), suffix.size()) == suffix;
  };

  bool tetgenio_info = false;
  bool igl_info = false;

  if (ends_with(filename, ".obj"))
  {
    igl_info = igl::readOBJ(filename, V_ori, F_ori);
    if (igl_info)
      tetgenio_info = igl::copyleft::tetgen::mesh_to_tetgenio(V_ori, F_ori, tetio);
    t_offset = 0;
  }

  if (ends_with(filename, ".off"))
  {
    igl_info = igl::readOFF(filename, V_ori, F_ori);
    tetgenio_info = tetio.load_off(f_tmp);
    t_offset = 0;
  }

  if (ends_with(filename, ".ply"))
  {
    igl_info = igl::readPLY(filename, V_ori, F_ori);
    tetgenio_info = tetio.load_ply(f_tmp);
    t_offset = 0;
  }
    
  if (ends_with(filename, ".stl"))
  {
    dMat N;
    igl_info = igl::readSTL(filename, V_ori, F_ori, N);
    tetgenio_info = tetio.load_stl(f_tmp);
    t_offset = 1;
  }

  delete[] f_tmp;

  if (igl_info)
  {
    show_origin();
  }
  else {
    printf("Fail to load file %s as triangular mesh.\n", filename.c_str());
  }

  if (tetgenio_info)
  {
    printf("Succeed to load file %s to tetgenio struct.\n", filename.c_str());
  }
  else {
    printf("Fail to load file %s to tetgenio struct\n", filename.c_str());
  }

  return tetgenio_info;
}

int tetrahedralize_tetgenio(tetgenio* in, std::string switches, dMat& V, iMat& T, iVec& TR)
{
  tetgenio out;
  using namespace std;
  try
  {
    char * cswitches = new char[switches.size() + 1];
    std::strcpy(cswitches, switches.c_str());
    ::tetrahedralize(cswitches, in, &out);
    delete[] cswitches;
  }
  catch (int e)
  {
    cerr << "^" << __FUNCTION__ << ": TETGEN CRASHED... KABOOOM!!!" << endl;
    return 1;
  }
  if (out.numberoftetrahedra == 0)
  {
    cerr << "^" << __FUNCTION__ << ": Tetgen failed to create tets" << endl;
    return 2;
  }

  // readout vertices
  if(out.pointlist == NULL)
  {
    printf("^tetgenio_to_tetmesh Error: point list is NULL\n");
    return false;
  }
  V.resize(out.numberofpoints, 3);
  for (int i = 0; i < out.numberofpoints; i++)
  {
    V(i, 0) = out.pointlist[i*3+0];
    V(i, 1) = out.pointlist[i*3+1];
    V(i, 2) = out.pointlist[i*3+2];
  }

  // readout tetrahedras
  if(out.tetrahedronlist == NULL)
  {
    printf("^tetgenio_to_tetmesh Error: tet list is NULL\n");
    return false;
  }
  assert(out.numberofcorners == 4);
  T.resize(out.numberoftetrahedra, 4);
  for (int i = 0; i < out.numberoftetrahedra; i++)
  {
    T(i, 0) = out.tetrahedronlist[i*4+0];
    T(i, 1) = out.tetrahedronlist[i*4+1];
    T(i, 2) = out.tetrahedronlist[i*4+2];
    T(i, 3) = out.tetrahedronlist[i*4+3];
  }

  T.array() -= min(t_offset, T.minCoeff());
  assert(T.maxCoeff() >= 0);
  assert(T.minCoeff() >= 0);
  assert(T.maxCoeff() < V.rows());

  // readout tetrahedra attributes
  if(out.tetrahedronattributelist == NULL)
  {
    printf("^tetgenio_to_tetmesh Error: tet attr list is NULL\n");
    return false;
  }
  TR.resize(out.numberoftetrahedra, 1);
  unordered_map<double, int> hashUniqueRegions;
  hashUniqueRegions.clear();
  int nR = 0;
  for (int i = 0; i < out.numberoftetrahedra; i++)
  {
    double attr = out.tetrahedronattributelist[i]; 
    if (hashUniqueRegions.find(attr) == hashUniqueRegions.end())
    {
      hashUniqueRegions[attr] = nR++;
    }
    TR(i, 0) = hashUniqueRegions[attr];
  }
  assert(hashUniqueRegions.size() == nR);
  assert(TR.minCoeff() >= 0);
  assert(TR.maxCoeff() < nR);
  
  return 0;
}

bool show_mesh_vertex(int vid)
{
  dMat v = V_ori.row(vid - 1);
  dMat c(1, 3);
  c << 1, 0, 0;
  viewer.data().clear_points();
  viewer.data().add_points(v, c);
  return true;
}

bool remove_unrefed = true;
bool export_tet_info = true;
bool export_file(const std::string filename)
{
  iMat T_exp;
  dMat V_exp;
  iMat TX_exp=TX_vis;
	
  if (remove_unrefed)
  {
    printf("Remove unreferenced vertices.\n");
    if (remove_unrefed)
    {
      iMat I, J;
      igl::remove_unreferenced(V_tet, T_vis, V_exp, T_exp, I, J);
    }
  }
  else
  {
    T_exp = T_vis;
    V_exp = V_tet;
  }

  printf("Writing as VTX format.\n");
  auto out = std::ofstream(filename, std::ofstream::out | std::ofstream::trunc);
  
  fmt::print(out, "# writing with .vtx format\n");

  // tetrahedron information number, which is its cluster info
  if (export_tet_info)
    fmt::print(out, "txn 1\n");

  for (int i = 0; i < V_exp.rows(); i++)
    fmt::print(out, "v {:f} {:f} {:f}\n", V_exp(i, 0), V_exp(i, 1), V_exp(i, 2));

  if (export_tet_info)
  {
    for (int i = 0; i < T_exp.rows(); i++)
      fmt::print(out, "t {:d} {:d} {:d} {:d} {:d}\n",
          T_exp(i, 0), T_exp(i, 1), T_exp(i, 2), T_exp(i, 3), TX_exp(i));
  }
  else
  {
    for (int i = 0; i < T_exp.rows(); i++)
      fmt::print(out, "t {:d} {:d} {:d} {:d}\n",
          T_exp(i, 0), T_exp(i, 1), T_exp(i, 2), T_exp(i, 3));
  }
	
  out.close();
  return true;
}

int main(int argc, char* argv[])
{
  std::string input_file;
  if (argc == 2)
    input_file = std::string(argv[1]);
  else
    input_file = "cylinder.ply";

  igl::opengl::glfw::imgui::ImGuiMenu menu;
  viewer.plugins.push_back(&menu);

  menu.callback_draw_viewer_menu = [&](){};

  menu.callback_draw_custom_window = [&]()
  {
    // Define next window position + size
    ImGui::SetNextWindowPos(ImVec2(0, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(
        "Tetgen Control Panel", nullptr,
        ImGuiWindowFlags_NoSavedSettings
    );

    // Load mesh
    if (ImGui::CollapsingHeader("Load", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // TODO file dialog
      ImGui::InputText("input file", input_file);
      if (ImGui::Button("Open", ImVec2(-1,0)))
      {
        load_tetgenio(input_file);
        // TODO error handle
      }
    }

    // tetrahedralize
    if (ImGui::CollapsingHeader("Tetgen", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // tetrahedralizaiton argument 
      static std::string para_str = "pqAa1e-1";
      ImGui::InputText("parameters", para_str);
      // TODO help menu
      // tetrahedralize
      if (ImGui::Button("Tetrahedralize", ImVec2(-1,0)))
      {
        int info;
        info = tetrahedralize_tetgenio(&tetio, para_str, V_tet, T_tet, TX_tet);
        if (info != 0)
        {
          printf("Fail to tetrahedralize mesh with argv [-%s]\n", para_str.c_str());
          return;
        }

        // switch tet to make it outward
        {
          Eigen::VectorXi l = T_tet.col(0);
          T_tet.col(0) = T_tet.col(1);
          T_tet.col(1) = l;
        }
        
        igl::boundary_facets(T_tet, F_tet);

        mask = bVec::Ones(TX_tet.maxCoeff() + 1);
        mask_prev = mask;

        printf("Tetrahedralize finished.\n");
        show_tet();
       }
    }

    // Debug
    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
      static int vid = 1;
      ImGui::InputInt("vertex", &vid);
      if (ImGui::Button("Show Vertex", ImVec2(-1, 0)))
      {
        if (show_mesh_vertex(vid))
        {
          printf("Show vertex %d\n", vid);
        }
      }
    }
    
    // Visualize
    if (ImGui::CollapsingHeader("Show Region", ImGuiTreeNodeFlags_DefaultOpen))
    {
      for (int i = 0; i < mask.size(); i++)
      {
        std::string label = fmt::format("Show region {:d}", i);
        ImGui::Checkbox(label.c_str(), &mask(i));
      }
      for (int i = 0; i < mask.size(); i++)
      {
        if (mask(i) != mask_prev(i))
        {
          mask_prev(i) = mask(i);
          show_tet();
        }
      }
    }

    // Export
    if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // Export options
      ImGui::Checkbox("remove unreferred", &remove_unrefed);
      ImGui::Checkbox("export region info", &export_tet_info);
      // TODO file dialog
      static std::string output_file = "test.vtx";
      ImGui::InputText("output file", output_file);
      if (ImGui::Button("Export", ImVec2(-1,0)))
      {
        if (export_file(output_file))
        {
          printf("Successfully write to %s\n", output_file.c_str());
        }
       }
    }

    ImGui::End();
  };

  tetgenbehavior b;
  b.syntax();

  viewer.launch();

  return 0;
}
