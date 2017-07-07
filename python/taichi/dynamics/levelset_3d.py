import taichi as tc
from taichi.misc.util import *


class LevelSet3D:
    def __init__(self, res, offset=None):
        if offset is None:
            offset = Vector(0.5, 0.5, 0.5)
        self.delta_x = 1.0 / min(res.x, res.y, res.z)
        self.res = res + Vectori(1, 1, 1)
        self.levelset = tc.core.LevelSet3D(self.res, offset)
        self.id = tc.core.register_levelset3d(self.levelset)

    def add_sphere(self, center, radius, inside_out=False):
        if type(center) != tc.core.Vector3:
            center = Vector(center[0], center[1], center[2])
        self.levelset.add_sphere(Vector(center.x / self.delta_x, center.y / self.delta_x, center.z / self.delta_x),
                                 radius / self.delta_x, inside_out)

    def add_plane(self, a, b, c, d):
        self.levelset.add_plane(Vector(a, b, c), d / self.delta_x)

    def add_cuboid(self, lower_boundry, upper_boundry, inside_out=False):
        self.levelset.add_cuboid(
            Vector(lower_boundry[0] / self.delta_x, lower_boundry[1] / self.delta_x, lower_boundry[2] / self.delta_x),
            Vector(upper_boundry[0] / self.delta_x, upper_boundry[1] / self.delta_x, upper_boundry[2] / self.delta_x),
            inside_out
        )

    def global_increase(self, delta):
        self.levelset.global_increase(delta / self.delta_x)

    # def get(self, x, y=None):
    #     if y is None:
    #         y = x.y
    #         x = x.x
    #     return self.levelset.sample(x / self.delta_x, y / self.delta_x)
    #
    # def get_normalized_gradient(self, p):
    #     p.x /= self.delta_x
    #     p.y /= self.delta_x
    #     return self.levelset.get_normalized_gradient(p)

    def set_friction(self, f):
        self.levelset.friction = f

    def get_delta_x(self):
        return self.delta_x
