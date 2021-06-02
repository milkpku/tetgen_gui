# Tetgen_gui 

Tetgen_gui is a tiny GUI wrapper for Tetgen. It takes in a 3D mesh in obj/ply/off/stl format and tetrahedralize it. 

![](tetgen_gui.gif)

## Compile
```bash
git clone https://github.com/milkpku/tetgen_gui.git
cd tetgen_gui && mkdir build && cd build
cmake .. && make
```

## Usage

1. **Load mesh**: type in input file name in `input file` and click `Open`.

2. **Tetrahedralize mesh**: type in parameter for tetgen in `parameters` and click `Tetrahedralize`.

3. **Select interested regions**: After tetrahedralization, tetrahedrons will be clustered into different regions. Check interested regions to select them. Only tetrahedrons in selected regions will be exported in next step.

4. **Output tetrahedron mesh**: type in output file name in `output file`, select whether remove unreferred vertices or export region information.

## Output Format
At the beginning of `.vtx` file, a line starts with `txn` specifies the number of attributes attached to each tetrahedron, here we take one channel to store region id of each tetrahedron when this information is required.

`.vtx` format is similar to `.obj` format, mainly contains two kinds of elements: 
- **Vertex:** starts with `v` with format `v x y z`, where `(x, y, z)` is the location of the vertex.
- **Tetrahedron:** starts with `t` with format `t x y z w <attributes>`, where `(x, y, z, w)` contains indices to four vertices, representing a tetrahedron, and the facet `(x, y, z)` faces outside. Note that the index for vertices starts with 0 rather than 1. `<attributes>` are attributes assigned to each tetrahedron. 

Here are two examples, corresponding to outputs with/without region information:

```python
# writing with .vtx format
v 0.000000 0.000000 0.000000
v 1.000000 0.000000 0.000000
v 0.000000 1.000000 0.000000
v 0.000000 0.000000 1.000000
t 1 2 3 0
```

```python
# writing with .vtx format
txn 1
v 0.000000 0.000000 0.000000
v 1.000000 0.000000 0.000000
v 0.000000 1.000000 0.000000
v 0.000000 0.000000 1.000000
t 1 2 3 0 1
```