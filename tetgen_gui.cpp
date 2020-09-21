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
#include <fmt/os.h>

igl::opengl::glfw::Viewer viewer;

typedef Eigen::MatrixXd dMat;
typedef Eigen::MatrixXi iMat;
typedef Eigen::VectorXi iVec;

// input visualization & representation
dMat V_ori;
iMat F_ori;

tetgenio tetio;

// output representation
dMat V_tet;
iMat T_tet;
iMat F_tet;
iVec TX_tet;

// visualize representation
iMat F_vis;

void show_origin()
{
  viewer.data().clear();
  viewer.data().set_mesh(V_ori, F_ori);
}

void show_tet()
{
  viewer.data().clear();
  igl::boundary_facets(T_tet, F_vis);
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
  }

  if (ends_with(filename, ".off"))
  {
    igl_info = igl::readOFF(filename, V_ori, F_ori);
    tetgenio_info = tetio.load_off(f_tmp);
  }

  if (ends_with(filename, ".ply"))
  {
    igl_info = igl::readPLY(filename, V_ori, F_ori);
    tetgenio_info = tetio.load_ply(f_tmp);
  }
    
  if (ends_with(filename, ".stl"))
  {
    dMat N;
    igl_info = igl::readSTL(filename, V_ori, F_ori, N);
    tetgenio_info = tetio.load_stl(f_tmp);
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

int tetrahedralize_tetgenio(tetgenio* in, std::string switches, dMat& V, iMat& T, iVec& TX, iMat& F)
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

  std::vector<std::vector<double>> TV;
  std::vector<std::vector<int>> TT;
  std::vector<std::vector<int>> TF;
  bool success = igl::copyleft::tetgen::tetgenio_to_tetmesh(out, TV, TT, TF);
  if (!success) return -1;

  V.resize(TV.size(), 3);
  for (int i = 0; i < TV.size(); i++)
    V.row(i) << TV[i][0], TV[i][1], TV[i][2];

  T.resize(TT.size(), 4);
  for (int i = 0; i < TT.size(); i++)
    T.row(i) << TT[i][0], TT[i][1], TT[i][2], TT[i][3];

  TX = iVec::Ones(T.rows());  //TODO read out TX info from tetgenio

  F.resize(TF.size(), 3);
  for (int i = 0; i < TF.size(); i++)
    F.row(i) << TF[i][0], TF[i][1], TF[i][2];

  return 0;
}

bool remove_unrefed = true;
bool export_surf = false;
bool export_tet = true;
bool export_file(const std::string filename)
{
  if (export_surf && !export_tet)
  {
    printf("Writing as OBJ format\n");
    if (remove_unrefed)
    {
      dMat NV;
      iMat NF, I;
      igl::remove_unreferenced(V_tet, F_tet, NV, NF, I);
      return igl::writeOBJ(filename, NV, NF);
    }
    else
    {
      return igl::writeOBJ(filename, V_tet, F_tet);
    }
  }

  printf("Writing as VFTX format\n");
  auto out = fmt::output_file(filename);
//if (!out.good())
//{
//  printf("fail to open file %s\n", filename.c_str());
//  return false;
//}
  
  out.print("# writing with .vftx format\n");
  out.print("txn 1\n");     // tetrahedron information number, which is its cluster info
  for (int i = 0; i < V_tet.rows(); i++)
    out.print("v {:f} {:f} {:f}\n", V_tet(i, 0), V_tet(i, 1), V_tet(i, 2));

  if (export_tet)
    for (int i = 0; i < T_tet.rows(); i++)
      out.print("t {:d} {:d} {:d} {:d} {:d}\n", T_tet(i, 0), T_tet(i, 1), T_tet(i, 2), T_tet(i, 3), TX_tet(i));

  if (export_surf)
    for (int i = 0; i < F_tet.rows(); i++)
      out.print("f {:d} {:d} {:d}\n", F_tet(i, 0), F_tet(i, 1), F_tet(i, 2));

  out.close();
  return true;
}

int main(int argc, char* argv[])
{
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
      static std::string mesh_file = "bunny.off";
      ImGui::InputText("input file", mesh_file);
      if (ImGui::Button("Open", ImVec2(-1,0)))
      {
        load_tetgenio(mesh_file);
        // TODO error handle
      }
    }

    // tetrahedralize
    if (ImGui::CollapsingHeader("Tetgen", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // tetrahedralizaiton argument 
      static std::string para_str = "pq";
      ImGui::InputText("parameters", para_str);
      // TODO help menu
      // tetrahedralize
      if (ImGui::Button("Tetrahedralize", ImVec2(-1,0)))
      {
        int info;
        info = tetrahedralize_tetgenio(&tetio, para_str, V_tet, T_tet, TX_tet, F_tet);
        if (info != 0)
        {
          printf("Fail to tetrahedralize mesh with argv [-%s]\n", para_str.c_str());
          return;
        }

        // switch facet to make it outward
        {
          Eigen::VectorXi l = F_tet.col(0);
          F_tet.col(0) = F_tet.col(1);
          F_tet.col(1) = l;
        }
        
        // switch tet to make it outward
        {
          Eigen::VectorXi l = T_tet.col(0);
          T_tet.col(0) = T_tet.col(1);
          T_tet.col(1) = l;
        }

        printf("Tetrahedralize finished.\n");
        show_tet();
       }
    }

    // Export
    if (ImGui::CollapsingHeader("Load", ImGuiTreeNodeFlags_DefaultOpen))
    {
      // Export options
      ImGui::Checkbox("remove unrefed", &remove_unrefed);
      ImGui::Checkbox("export surf", &export_surf);
      ImGui::Checkbox("export tet", &export_tet);
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

  viewer.launch();

  return 0;
}
