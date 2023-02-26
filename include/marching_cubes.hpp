// Author: Zackery Mason-Blaug
// Date: 2022-10-18
//////////////////////////////////////////////////////////


#pragma once

#include "types.hpp"

#include "Graphics/buffer.hpp"

#include "Util/allocators.hpp"

#include <functional>


namespace marching_cubes {

    struct vertex_t {
        v3f pos;
        v3f nrm;
    };

    struct grid_cell_t {
        v3f p[8];
        f32 v[8];
    };

    struct triangle_t {
        v3f p[3];
    };
    int to_polygon(const grid_cell_t& grid, f32 iso_level, triangle_t* triangles);
};

