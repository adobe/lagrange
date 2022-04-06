#include <lagrange/ui/utils/immediate.h>

#include <lagrange/ui/components/GLMesh.h>
#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/MeshRender.h>
#include <lagrange/ui/default_shaders.h>


namespace lagrange {
namespace ui {

struct ImmediateEntities
{
    ui::Entity points;
    std::vector<Eigen::Vector3f> points_pos;
    std::vector<Eigen::Vector3f> points_color;

    ui::Entity lines;
    std::vector<Eigen::Vector3f> lines_pos;
    std::vector<Eigen::Vector3f> lines_color;

    void reset()
    {
        points_pos.clear();
        points_color.clear();
        lines_pos.clear();
        lines_color.clear();
    }
};


ImmediateEntities& get_immediate_entities(Registry& r)
{
    auto ptr = r.try_ctx<ImmediateEntities>();
    if (ptr) return *ptr;

    ImmediateEntities ie;

    ie.points = r.create();
    {
        GLMesh gl;
        gl.attribute_buffers[DefaultShaderAtrribNames::Position] = std::make_shared<GPUBuffer>();
        r.emplace<GLMesh>(ie.points, std::move(gl));

        MeshRender mr;
        mr.indexing = IndexingMode::VERTEX;
        mr.primitive = PrimitiveType::POINTS;
        mr.material = std::make_shared<Material>(r, DefaultShaders::Simple);
        mr.material->set_float(RasterizerOptions::PointSize, 4.0f);
        r.emplace<MeshRender>(ie.points, mr);
    }


    ie.lines = r.create();
    {
        GLMesh gl;
        gl.attribute_buffers[DefaultShaderAtrribNames::Position] = std::make_shared<GPUBuffer>();
        r.emplace<GLMesh>(ie.lines, std::move(gl));

        MeshRender mr;
        mr.indexing = IndexingMode::VERTEX;
        mr.primitive = PrimitiveType::LINES;
        mr.material = std::make_shared<Material>(r, DefaultShaders::Simple);
        r.emplace<MeshRender>(ie.lines, mr);
    }

    return r.set<ImmediateEntities>(std::move(ie));
}

void render_points(Registry& r, const std::vector<Eigen::Vector3f>& points)
{
    auto& ie = get_immediate_entities(r);
    ie.points_pos.insert(std::end(ie.points_pos), std::begin(points), std::end(points));
}

void render_point(Registry& r, const Eigen::Vector3f& point)
{
    auto& ie = get_immediate_entities(r);
    ie.points_pos.push_back(point);
}

void render_lines(Registry& r, const std::vector<Eigen::Vector3f>& lines)
{
    auto& ie = get_immediate_entities(r);
    ie.lines_pos.insert(std::end(ie.lines_pos), std::begin(lines), std::end(lines));
}

void upload_immediate_system(Registry& r)
{
    auto& ie = get_immediate_entities(r);

    r.get<GLMesh>(ie.points)
        .get_attribute_buffer(DefaultShaderAtrribNames::Position)
        ->vbo()
        .upload(
            static_cast<GLuint>(ie.points_pos.size() * sizeof(Eigen::Vector3f)),
            (const unsigned char*)ie.points_pos.data(),
            static_cast<GLsizei>(ie.points_pos.size()),
            false,
            GL_FLOAT);

    r.get<GLMesh>(ie.lines)
        .get_attribute_buffer(DefaultShaderAtrribNames::Position)
        ->vbo()
        .upload(
            static_cast<GLuint>(ie.lines_pos.size() * sizeof(Eigen::Vector3f)),
            (const unsigned char*)ie.lines_pos.data(),
            static_cast<GLsizei>(ie.lines_pos.size() / 2),
            false,
            GL_FLOAT);


    ie.reset();
}

} // namespace ui
} // namespace lagrange