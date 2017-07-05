/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2016 Yuanming Hu <yuanmhu@gmail.com>
                  2017 Yu Fang <squarefk@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#include <taichi/python/export.h>
#include <taichi/common/config.h>
#include <taichi/math/levelset.h>
#include <taichi/visualization/rgb.h>
#include <taichi/math/array_op.h>
#include <taichi/math/dynamic_levelset_2d.h>
#include <taichi/math/dynamic_levelset_3d.h>

PYBIND11_MAKE_OPAQUE(std::vector<int>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::real>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector2>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector3>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector4>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector2i>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector3i>);
PYBIND11_MAKE_OPAQUE(std::vector<taichi::Vector4i>);

TC_NAMESPACE_BEGIN
std::vector<real> make_range(real start, real end, real delta) {
    return std::vector<real> {start, end, delta};
}

template<typename T, int ret>
int return_constant(T *) { return ret; }

template<typename T, int channels>
void ndarray_to_image_buffer(T *arr, uint64 input, int width, int height) // 'input' is actually a pointer...
{
    arr->initialize(width, height);
    for (auto &ind : arr->get_region()) {
        for (int i = 0; i < channels; i++) {
            (*arr)[ind][i] = reinterpret_cast<real *>(input)[ind.i * channels * height + ind.j * channels + i];
        }
    }
}


std::string rasterize_levelset(const LevelSet2D &levelset, int width, int height) {
    std::string ret;
    for (auto &ind : Region2D(0, width, 0, height)) {
        real c = -levelset.sample((ind.i + 0.5f) / width * levelset.get_width(),
                                  (ind.j + 0.5f) / height * levelset.get_height());
        RGB rgb(c, c, c);
        rgb.append_to_string(ret);
    }
    return ret;
}

Matrix4 matrix4_translate(Matrix4 *transform, const Vector3 &offset) {
    return glm::translate(Matrix4(1.0f), offset) * *transform;
}

Matrix4 matrix4_scale(Matrix4 *transform, const Vector3 &scales) {
    return glm::scale(Matrix4(1.0f), scales) * *transform;
}

Matrix4 matrix4_scale_s(Matrix4 *transform, real s) {
    return matrix4_scale(transform, Vector3(s));
}

Matrix4 matrix4_rotate_angle_axis(Matrix4 *transform, real angle, const Vector3 &axis) {
    return glm::rotate(Matrix4(1.0f), angle * pi / 180.0f, axis) * *transform;
}

Matrix4 matrix4_rotate_euler(Matrix4 *transform, const Vector3 &euler_angles) {
    Matrix4 ret = *transform;
    ret = matrix4_rotate_angle_axis(&ret, euler_angles.x, Vector3(1.0f, 0.0f, 0.0f));
    ret = matrix4_rotate_angle_axis(&ret, euler_angles.y, Vector3(0.0f, 1.0f, 0.0f));
    ret = matrix4_rotate_angle_axis(&ret, euler_angles.z, Vector3(0.0f, 0.0f, 1.0f));
    return ret;
}

template<typename T, int channels>
void ndarray_to_array2d(T *arr, long long input, int width, int height) // 'input' is actually a pointer...
{
    arr->initialize(width, height);
    for (auto &ind : arr->get_region()) {
        for (int i = 0; i < channels; i++) {
            (*arr)[ind][i] = reinterpret_cast<float *>(input)[ind.i * height * channels + ind.j * channels + i];
        }
    }
}

void
ndarray_to_array2d_real(Array2D<real> *arr, long long input, int width, int height) // 'input' is actually a pointer...
{
    arr->initialize(Vector2i(width, height));
    for (auto &ind : arr->get_region()) {
        (*arr)[ind] = reinterpret_cast<float *>(input)[ind.i * height + ind.j];
    }
}

template<typename T, int channels>
void array2d_to_ndarray(T *arr, uint64 output) // 'output' is actually a pointer...
{
    int width = arr->get_width(), height = arr->get_height();
    for (auto &ind : arr->get_region()) {
        for (int k = 0; k < channels; k++)
            reinterpret_cast<real *>(output)[ind.i * height * channels + ind.j * channels + k] =
                    *(((float *)(&(*arr)[ind])) + k);
    }
}

void export_math(py::module &m) {
    m.def("rasterize_levelset", rasterize_levelset);

    py::class_<Config>(m, "Config");

#define EXPORT_ARRAY_2D_OF(T, C) \
    py::class_<Array2D<real>>(m, "Array2D" #T)  \
            .def(py::init<int, int>()) \
            .def("to_ndarray", &array2d_to_ndarray<Array2D<T>, C>) \
            .def("get_width", &Array2D<T>::get_width) \
            .def("get_height", &Array2D<T>::get_height) \
            .def("rasterize", &Array2D<T>::rasterize) \
            .def("rasterize_scale", &Array2D<T>::rasterize_scale) \
            .def("from_ndarray", &ndarray_to_array2d_real);

    EXPORT_ARRAY_2D_OF(real, 1);

#define EXPORT_ARRAY_3D_OF(T, C) \
    py::class_<Array3D<real>> PyArray3D##T(m, "Array3D" #T);  \
        PyArray3D##T.def(py::init<int, int, int>()) \
            .def("get_width", &Array3D<T>::get_width) \
            .def("get_height", &Array3D<T>::get_height);

    EXPORT_ARRAY_3D_OF(real, 1);

    py::class_<Array2D<Vector3>>(m, "Array2DVector3")
            .def(py::init<int, int, Vector3>())
            .def("get_width", &Array2D<Vector3>::get_width)
            .def("get_height", &Array2D<Vector3>::get_height)
            .def("get_channels", &return_constant<Array2D<Vector3>, 3>)
            .def("from_ndarray", &ndarray_to_image_buffer<Array2D<Vector3>, 3>)
            .def("read", &Array2D<Vector3>::load)
            .def("write", &Array2D<Vector3>::write)
            .def("write_to_disk", &Array2D<Vector3>::write_to_disk)
            .def("read_from_disk", &Array2D<Vector3>::read_from_disk)
            .def("rasterize", &Array2D<Vector3>::rasterize)
            .def("rasterize_scale", &Array2D<Vector3>::rasterize_scale)
            .def("to_ndarray", &array2d_to_ndarray<Array2D<Vector3>, 3>);

    py::class_<Array2D<Vector4>>(m, "Array2DVector4")
            .def(py::init<int, int, Vector4>())
            .def("get_width", &Array2D<Vector4>::get_width)
            .def("get_height", &Array2D<Vector4>::get_height)
            .def("get_channels", &return_constant<Array2D<Vector4>, 4>)
            .def("write", &Array2D<Vector4>::write)
            .def("from_ndarray", &ndarray_to_image_buffer<Array2D<Vector4>, 4>)
            .def("write_to_disk", &Array2D<Vector4>::write_to_disk)
            .def("read_from_disk", &Array2D<Vector4>::read_from_disk)
            .def("rasterize", &Array2D<Vector4>::rasterize)
            .def("rasterize_scale", &Array2D<Vector4>::rasterize_scale)
            .def("to_ndarray", &array2d_to_ndarray<Array2D<Vector4>, 4>);

    py::class_<LevelSet2D, Array2D<real>>(m, "LevelSet2D")
            .def(py::init<int, int, Vector2>())
            .def("get_width", &LevelSet2D::get_width)
            .def("get_height", &LevelSet2D::get_height)
            .def("get", &LevelSet2D::get_copy)
            .def("set", static_cast<void (LevelSet2D::*)(int, int, const real &)>(&LevelSet2D::set))
            .def("add_sphere", &LevelSet2D::add_sphere)
            .def("add_polygon", &LevelSet2D::add_polygon)
            .def("get_gradient", &LevelSet2D::get_gradient)
            .def("rasterize", &LevelSet2D::rasterize)
            .def("sample", static_cast<real(LevelSet2D::*)(real, real) const>(&LevelSet2D::sample))
            .def("get_normalized_gradient", &LevelSet2D::get_normalized_gradient)
            .def("to_ndarray", &array2d_to_ndarray<LevelSet2D, 1>)
            .def("get_channels", &return_constant<LevelSet2D, 1>)
            .def_readwrite("friction", &LevelSet2D::friction);

    py::class_<DynamicLevelSet3D>(m, "DynamicLevelSet3D")
            .def(py::init<>())
            .def("initialize", &DynamicLevelSet3D::initialize);

    py::class_<LevelSet3D, std::shared_ptr<LevelSet3D>>(m, "LevelSet3D", PyArray3Dreal)
            .def(py::init<int, int, int, Vector3>())
            .def("get_width", &LevelSet3D::get_width)
            .def("get_height", &LevelSet3D::get_height)
            .def("get", &LevelSet3D::get_copy)
            .def("set", static_cast<void (LevelSet3D::*)(int, int, int, const real &)>(&LevelSet3D::set))
            .def("add_sphere", &LevelSet3D::add_sphere)
            .def("add_plane", &LevelSet3D::add_plane)
            .def("add_cuboid", &LevelSet3D::add_cuboid)
            .def("global_increase", &LevelSet3D::global_increase)
            .def("get_gradient", &LevelSet3D::get_gradient)
            .def("rasterize", &LevelSet3D::rasterize)
            .def("sample", static_cast<real(LevelSet3D::*)(real, real, real) const>(&LevelSet3D::sample))
            .def("get_normalized_gradient", &LevelSet3D::get_normalized_gradient)
            .def("get_channels", &return_constant<LevelSet3D, 1>)
            .def_readwrite("friction", &LevelSet3D::friction);

    py::class_<DynamicLevelSet2D>(m, "DynamicLevelSet2D")
            .def(py::init<>())
            .def("initialize", &DynamicLevelSet2D::initialize);

    m.def("points_inside_polygon", points_inside_polygon);
    m.def("points_inside_sphere", points_inside_sphere);
    m.def("make_range", make_range);

    py::class_<Matrix4>(m, "Matrix4")
            .def(py::init<real>())
            .def(real() * py::self)
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * py::self)
            .def(py::self / py::self)
            .def("translate", &matrix4_translate)
            .def("scale", &matrix4_scale)
            .def("scale_s", &matrix4_scale_s)
            .def("rotate_euler", &matrix4_rotate_euler)
            .def("rotate_angle_axis", &matrix4_rotate_angle_axis)
            .def("get_ptr_string", &Config::get_ptr_string<Matrix4>);

    m.def("gaussian_blur_x_2d_real", gaussian_blur_x<real>);
    m.def("gaussian_blur_y_2d_real", gaussian_blur_y<real>);
    m.def("gaussian_blur_2d_real", gaussian_blur<real>);

    py::class_<Vector2i>(m, "Vector2i")
            .def(py::init<int, int>())
            .def_readwrite("x", &Vector2i::x)
            .def_readwrite("y", &Vector2i::y)
            .def(py::self * int())
            .def(int() * py::self)
            .def(py::self / int())
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * py::self)
            .def(py::self / py::self);

    py::class_<Vector2>(m, "Vector2")
            .def(py::init<real, real>())
            .def_readwrite("x", &Vector2::x)
            .def_readwrite("y", &Vector2::y)
            .def(py::self * real())
            .def(real() * py::self)
            .def(py::self / real())
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * py::self)
            .def(py::self / py::self);

    py::class_<Vector3i>(m, "Vector3i")
            .def(py::init<int, int, int>())
            .def_readwrite("x", &Vector3i::x)
            .def_readwrite("y", &Vector3i::y)
            .def_readwrite("z", &Vector3i::z)
            .def(py::self * int())
            .def(int() * py::self)
            .def(py::self / int())
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self * py::self)
            .def(py::self / py::self);

    py::class_<Vector3>(m, "Vector3")
            .def(py::init<real, real, real>())
            .def_readwrite("x", &Vector3::x)
            .def_readwrite("y", &Vector3::y)
            .def_readwrite("z", &Vector3::z)
            .def(py::self * real())
            .def(real() * py::self)
            .def(py::self / real())
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(-py::self)
            .def(py::self * py::self)
            .def(py::self / py::self);

    py::class_<Vector4>(m, "Vector4")
            .def(py::init<real, real, real, real>())
            .def_readwrite("x", &Vector4::x)
            .def_readwrite("y", &Vector4::y)
            .def_readwrite("z", &Vector4::z)
            .def_readwrite("w", &Vector4::w)
            .def(py::self * real())
            .def(real() * py::self)
            .def(py::self / real())
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(-py::self)
            .def(py::self * py::self)
            .def(py::self / py::self);

    DEFINE_VECTOR_OF(real);
    DEFINE_VECTOR_OF(int);
    DEFINE_VECTOR_OF(Vector2);
    DEFINE_VECTOR_OF(Vector3);
    DEFINE_VECTOR_OF(Vector4);
    DEFINE_VECTOR_OF(Vector2i);
    DEFINE_VECTOR_OF(Vector3i);
    DEFINE_VECTOR_OF(Vector4i);
}

TC_NAMESPACE_END
