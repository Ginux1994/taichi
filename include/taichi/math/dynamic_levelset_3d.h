/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2017 Yu Fang <squarefk@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#pragma once

#include <limits>
#include <memory>
#include <taichi/math/levelset.h>

TC_NAMESPACE_BEGIN

class DynamicLevelSet3D {
public:
    real t0, t1;
    std::shared_ptr<LevelSet3D> levelset0, levelset1;

    void initialize(real _t0, real _t1, const LevelSet3D &_ls0, const LevelSet3D &_ls1);

    bool inside(const Vector3i pos) const {
        return levelset0->inside(pos.cast<real>());
    }

    bool inside(const Vector3 pos) const {
        return levelset0->inside(pos);
    }

    Vector3 get_spatial_gradient(const Vector3 &pos, real t) const;

    real get_temporal_derivative(const Vector3 &pos, real t) const;

    real sample(const Vector3 &pos, real t) const;

    Array3D<real> rasterize(int width, int height, int depth, real t);
};

TC_NAMESPACE_END

