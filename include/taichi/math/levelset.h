/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2017 Yuanming Hu <yuanmhu@gmail.com>
                  2017 Yu Fang <squarefk@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#pragma once

#include <taichi/common/util.h>
#include <taichi/math/array.h>
#include <taichi/math/linalg.h>

TC_NAMESPACE_BEGIN

template<int DIM>
class LevelSet : public ArrayND<DIM, real> {
public:
    using VectorI = VectorND<DIM, int>;
    using Vector = VectorND<DIM, real>;
    using Array = ArrayND<DIM, real>;
    static constexpr real INF = 1e7f;

    real friction = 1.0f;

    LevelSet() : Array(VectorI(0)) {}

    void initialize(const VectorI &res, Vector offset=Vector(0.5f), real value=INF) {
        Array::initialize(res, value, offset);
    }

    // 2D

    void add_sphere(Vector center, real radius, bool inside_out = false);

    void add_polygon(std::vector<Vector2> polygon, bool inside_out = false);

    real get(const Vector &pos) const;

    Array rasterize(VectorI output_res);

    // 3D

    void add_plane(const Vector &normal, real d);

    void add_cuboid(Vector3 lower_boundry, Vector3 upper_boundry, bool inside_out = true);

    void global_increase(real delta);

    Vector get_gradient(const Vector &pos) const; // Note this is not normalized!

    Vector get_normalized_gradient(const Vector &pos) const;

    static real fraction_inside(real phi_a, real phi_b) {
        if (phi_a < 0 && phi_b < 0)
            return 1;
        if (phi_a < 0 && phi_b >= 0)
            return phi_a / (phi_a - phi_b);
        if (phi_a >= 0 && phi_b < 0)
            return phi_b / (phi_b - phi_a);
        else
            return 0;
    }

    static real fraction_outside(real phi_a, real phi_b) {
        return 1.0f - fraction_inside(phi_a, phi_b);
    }
};

typedef LevelSet<2> LevelSet2D;
typedef LevelSet<3> LevelSet3D;

TC_NAMESPACE_END
