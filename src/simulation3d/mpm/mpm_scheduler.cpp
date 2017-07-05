/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2017 Yuanming Hu <yuanmhu@gmail.com>
                  2017 Yu Fang <squarefk@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#include "mpm_scheduler.h"

TC_NAMESPACE_BEGIN

template <int DIM>
void MPMScheduler<DIM>::expand(bool expand_vel, bool expand_state) {
    Array<int> new_states;
    Array<int> old_states;
    if (expand_state) {
        old_states = states;
    }
    min_vel_expanded = Vector3(1e30f, 1e30f, 1e30f);
    max_vel_expanded = Vector3(-1e30f, -1e30f, -1e30f);
    new_states.initialize(res, 0);

    auto update = [&](const Index3D ind, int dx, int dy, int dz,
                      const Array<Vector3> &min_vel,
                      const Array<Vector3> &max_vel,
                      Array<Vector3> &new_min_vel,
                      Array<Vector3> &new_max_vel,
                      const Array<int> &states, Array<int> &new_states) -> void {
        if (expand_vel) {
            auto &tmp_min = new_min_vel[ind.neighbour(dx, dy, dz)];
            tmp_min[0] = std::min(tmp_min[0], min_vel[ind][0]);
            tmp_min[1] = std::min(tmp_min[1], min_vel[ind][1]);
            tmp_min[2] = std::min(tmp_min[2], min_vel[ind][2]);
            auto &tmp_max = new_max_vel[ind.neighbour(dx, dy, dz)];
            tmp_max[0] = std::max(tmp_max[0], max_vel[ind][0]);
            tmp_max[1] = std::max(tmp_max[1], max_vel[ind][1]);
            tmp_max[2] = std::max(tmp_max[2], max_vel[ind][2]);
        }
        if (expand_state) {
            if (states[ind])
                new_states[ind.neighbour(dx, dy, dz)] = 1;
        }
    };

    for (auto &ind : states.get_region()) {
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (states.inside(ind.neighbour(dx, dy, dz)))
                        update(ind, dx, dy, dz, min_vel, max_vel, min_vel_expanded, max_vel_expanded, states,
                               new_states);
                }
            }
        }
    }
    if (expand_state) {
        states = new_states;
        states += old_states;
    } // 1: buffer, 2: updating
}

template <int DIM>
void MPMScheduler<DIM>::update() {
    // Use <= here since grid_res = sim_res + 1
    active_particles.clear();
    active_grid_points.clear();
    for (int i = 0; i <= sim_res[0]; i++) {
        for (int j = 0; j <= sim_res[1]; j++) {
            for (int k = 0; k <= sim_res[2]; k++) {
                if (states[i / grid_block_size][j / grid_block_size][k / grid_block_size] != 0) {
                    active_grid_points.push_back(Vector3i(i, j, k));
                }
            }
        }
    }
    for (auto &ind : states.get_region()) {
        if (states[ind] != 0) {
            for (auto &p : particle_groups[res[2] * res[1] * ind.i + res[2] * ind.j + ind.k]) {
                active_particles.push_back(p);
            }
        }
    }
    update_particle_states();
    // TODO: testing memory locality...
    // std::random_shuffle(active_particles.begin(), active_particles.end());
    /*
    std::sort(active_particles.begin(), active_particles.end(),
              [](MPM3Particle *a, MPM3Particle *b) {
                  return a->key() < b->key();
              });
    for (auto &p :active_particles) {
        p->pos = Vector3(10.0f);
        p->v = Vector3(0.0f);
    }
    */
}

template <int DIM>
int64 MPMScheduler<DIM>::update_max_dt_int(int64 t_int) {
    int64 ret = 1LL << 60;
    for (auto &ind : max_dt_int.get_region()) {
        int64 this_step_limit = std::min(max_dt_int_cfl[ind], max_dt_int_strength[ind]);
        int64 allowed_multiplier = 1;
        if (t_int % max_dt_int[ind] == 0) {
            allowed_multiplier = 2;
        }
        max_dt_int[ind] = std::min(max_dt_int[ind] * allowed_multiplier, this_step_limit);
        if (has_particle(ind)) {
            ret = std::min(ret, max_dt_int[ind]);
        }
    }
    return ret;
}

template <int DIM>
void MPMScheduler<DIM>::update_particle_groups() {
    // Remove all updating particles, and then re-insert them
    for (auto &ind : states.get_region()) {
        if (states[ind] == 0) {
            continue;
        }
        particle_groups[res[2] * res[1] * ind.i + res[2] * ind.j + ind.k].clear();
        updated[ind] = 1;
    }
    for (auto &p : active_particles) {
        insert_particle(p);
    }
}

template <int DIM>
void MPMScheduler<DIM>::insert_particle(MPMParticle<DIM> *p, bool is_new_particle) {
    int x = int(p->pos.x / grid_block_size);
    int y = int(p->pos.y / grid_block_size);
    int z = int(p->pos.z / grid_block_size);
    if (states.inside(x, y, z)) {
        int index = res[2] * res[1] * x + res[2] * y + z;
        particle_groups[index].push_back(p);
        updated[x][y][z] = 1;
        if (is_new_particle) {
            max_dt_int[x][y][z] = 1;
            active_particles.push_back(p);
        }
    }
}

template <int DIM>
void MPMScheduler<DIM>::update_dt_limits(real t) {
    for (auto &ind : states.get_region()) {
        // Update those blocks needing an update
        if (!updated[ind]) {
            continue;
        }
        updated[ind] = 0;
        max_dt_int_strength[ind] = 1LL << 60;
        max_dt_int_cfl[ind] = 1LL << 60;
        min_vel[ind] = Vector3(1e30f, 1e30f, 1e30f);
        max_vel[ind] = Vector3(-1e30f, -1e30f, -1e30f);
        for (auto &p : particle_groups[res[2] * res[1] * ind.i + res[2] * ind.j + ind.k]) {
            int64 march_interval;
            int64 allowed_t_int_inc = (int64)(strength_dt_mul * p->get_allowed_dt() / base_delta_t);
            if (allowed_t_int_inc <= 0) {
                P(allowed_t_int_inc);
                allowed_t_int_inc = 1;
            }
            march_interval = get_largest_pot(allowed_t_int_inc);
            max_dt_int_strength[ind] = std::min(max_dt_int_strength[ind],
                                                march_interval);
            auto &tmp_min = min_vel[ind];
            tmp_min[0] = std::min(tmp_min[0], p->v.x);
            tmp_min[1] = std::min(tmp_min[1], p->v.y);
            tmp_min[2] = std::min(tmp_min[2], p->v.z);
            auto &tmp_max = max_vel[ind];
            tmp_max[0] = std::max(tmp_max[0], p->v.x);
            tmp_max[1] = std::max(tmp_max[1], p->v.y);
            tmp_max[2] = std::max(tmp_max[2], p->v.z);
        }
    }
    // Expand velocity
    expand(true, false);

    for (auto &ind : min_vel.get_region()) {
        real block_vel = std::max(
                std::max(
                        max_vel_expanded[ind][0] - min_vel_expanded[ind][0],
                        max_vel_expanded[ind][1] - min_vel_expanded[ind][1]),
                max_vel_expanded[ind][2] - min_vel_expanded[ind][2]
        ) + 1e-7f;
        if (block_vel < 0) {
            // Blocks with no particles
            continue;
        }
        int64 cfl_limit = int64(cfl / block_vel / base_delta_t);
        if (cfl_limit <= 0) {
            P(cfl_limit);
            cfl_limit = 1;
        }
        real block_absolute_vel = 1e-7f;
        for (int i = 0; i < 3; i++) {
            block_absolute_vel = std::max(block_absolute_vel, std::abs(min_vel_expanded[ind][i]));
            block_absolute_vel = std::max(block_absolute_vel, std::abs(max_vel_expanded[ind][i]));
        }
        Vector3 levelset_query_position = Vector3(ind.get_pos() * real(grid_block_size));

        real last_distance;
        if (levelset->inside(levelset_query_position)) {
            last_distance = levelset->sample(levelset_query_position, t);
        } else {
            last_distance = 0.0f;
        }
        if (last_distance < LevelSet3D::INF) {
            real distance2boundary = std::max(last_distance - real(grid_block_size) * 0.75f, 0.5f);
            int64 boundary_limit = int64(cfl * distance2boundary / block_absolute_vel / base_delta_t);
            cfl_limit = std::min(cfl_limit, boundary_limit);
        }
        max_dt_int_cfl[ind] = get_largest_pot(cfl_limit);
    }
}

template <int DIM>
void MPMScheduler<DIM>::update_particle_states() {
    for (auto &p : get_active_particles()) {
        Vector3i low_res_pos(
                int(p->pos.x / grid_block_size),
                int(p->pos.y / grid_block_size),
                int(p->pos.z / grid_block_size)
        );
        if (states[low_res_pos] == 2) {
            p->color = Vector3(1.0f);
            p->state = Particle::UPDATING;
        } else {
            p->color = Vector3(0.7f);
            p->state = Particle::BUFFER;
        }
    }
}

template <int DIM>
void MPMScheduler<DIM>::reset_particle_states() {
    for (auto &p : get_active_particles()) {
        p->state = Particle::INACTIVE;
        p->color = Vector3(0.3f);
    }
}

template <int DIM>
void MPMScheduler<DIM>::enforce_smoothness(int64 t_int_increment) {
    Array<int64> new_max_dt_int = max_dt_int;
    for (auto &ind : states.get_region()) {
        if (states[ind] != 0) {
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        auto neighbour_ind = ind.neighbour(dx, dy, dz);
                        if (max_dt_int.inside(neighbour_ind)) {
                            new_max_dt_int[ind] = std::min(new_max_dt_int[ind], max_dt_int[neighbour_ind] * 2);
                        }
                    }
                }
            }
        }
    }
    max_dt_int = new_max_dt_int;
}

template
class MPMScheduler<3>;

TC_NAMESPACE_END
