
## Table of Contents
- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Geometry loading and registration](#geometry-loading-and-registration)
  - [Loading mesh](#loading-mesh)
  - [Retrieving and interacting with the mesh](#retrieving-and-interacting-with-the-mesh)
  - [Loading scene](#loading-scene)
- [Adding geometry to scene](#adding-geometry-to-scene)
  - [Default Physically Based Render (PBR)](#default-physically-based-render-pbr)
  - [Mesh visualizations](#mesh-visualizations)
    - [`GlyphType::Surface`](#glyphtypesurface)
    - [Colormaps](#colormaps)
- [Materials](#materials)
  - [Color/Texture Material Properties](#colortexture-material-properties)
    - [PBRMaterial](#pbrmaterial)
  - [Rasterizer Properties](#rasterizer-properties)
  - [Custom Shader Properties](#custom-shader-properties)
- [Common components](#common-components)
  - [`Name`](#name)
  - [`Transform`](#transform)
  - [`Tree`](#tree)
  - [`MeshGeometry`](#meshgeometry)
  - [`Hovered` and `Selected`](#hovered-and-selected)
  - [`Layer`](#layer)
  - [`UIPanel`](#uipanel)
  - [`Viewport`](#viewport)
- [User Interface Panels](#user-interface-panels)
- [Viewports](#viewports)
  - [Entity visibility](#entity-visibility)
  - [Multi viewport](#multi-viewport)
- [Entity Component System](#entity-component-system)
  - [Registry](#registry)
  - [Entity](#entity)
  - [Components](#components)
    - [Tag Components](#tag-components)
  - [Systems](#systems)
  - [Context variables](#context-variables)
  - [Design considerations](#design-considerations)
- [Customizing Lagrange UI](#customizing-lagrange-ui)
  - [Components](#components-1)
  - [Tools](#tools)
  - [Geometry](#geometry)
  - [Rendering](#rendering)
    - [Shader and Material properties](#shader-and-material-properties)
- [Examples](#examples)


## Overview

Lagrange UI uses an **Entity-Component-System (ECS)** architecture:
- Entity is a unique identifier
- Components define data and behavior (but no logic)
- Systems define logic (but no data).

See *[ECS implementation section](#entity-component-system)* for more information about ECS and how it's implemented in Lagrange UI. The underlying library for ECS is [`entt`](https://github.com/skypjack/entt).


Recommended namespace usage
```c++
namespace ui = lagrange::ui;
```

The entry point to the library is the `Viewer` class. It instantiates a window and owns a `Registry` instance and `Systems` instance. `Registry` contains all the data (entities, components) and `Systems` contain all the behavior (sequence of functions that is called every frame). To start the UI:

```c++
ui::Viewer viewer;
viewer.run([](){
    //Main loop code
});

//Or

viewer.run([](ui::Registry & r){
    //Main loop code
    return should_continue_running;
});
```

The API to interact with the UI follows this pattern
```c++
ui::Entity entity = ui::do_something(registry, params)
SomeData & data = registry.get<SomeData>(entity);
```

For example:
```c++
//Loads mesh from path
ui::Entity mesh_geometry = ui::load_mesh(registry, path);

//Adds the mesh to scene
ui::Entity mesh_visualization = ui::show_mesh(registry, mesh_geometry);

//Retrieves Transform component of the visualized mesh
Transform & transform = registry.get<Transform>(mesh_visualization);
```

All entities and their components live in a `Registry`. To access/set/modify the entities and components, use the `Viewer::registry()`.
```c++
auto & registry = viewer.registry();
auto entity = registry.create();
registry.emplace<MyPositionComponent>(entity, MyPositionComponent(0,0,0));
```

*TODO: lifetime discussion*

## Geometry loading and registration

### Loading mesh

Creates and entity that represents the mesh. This entity is only a resource - it is not rendered.
It can be referenced by components that need this geometry for rendering/picking/etc.
These entities have `MeshData` component attached that contains a `lagrange::MeshBase` pointer.

```c++
ui::Entity mesh_from_disk = ui::load_mesh(registry, path);
ui::Entity mesh_from_memory = ui::register_mesh(registry, lagrange::create_sphere());
```

### Retrieving and interacting with the mesh

To retrieve a mesh:
```c++
MeshType & mesh = ui::get_mesh<MeshType>(registry, mesh_entity);
```

There are several methods that do not require the knowledge of the mesh type. These may however incur copy and conversion costs.
```c++
RowMajorMatrixXf get_mesh_vertices(const MeshData& d);
RowMajorMatrixXf get_mesh_facets(const MeshData& d);
bool has_mesh_vertex_attribute(const MeshData& d, const std::string& name);
bool has_mesh_facet_attribute(const MeshData& d, const std::string& name);
...
RowMajorMatrixXf get_mesh_vertex_attribute(const MeshData& d, const std::string& name);
RowMajorMatrixXf get_mesh_facet_attribute(const MeshData& d, const std::string& name);
...
std::optional<RayFacetHit> intersect_ray(const MeshData& d, const Eigen::Vector3f& origin, const Eigen::Vector3f& dir);
...
```


### Loading scene
Loads a scene using Assimp. Creates a hierarchy of entities and loads meshes, materials and textures. Returns the top-level entity.

```c++
ui::Entity root = ui::load_scene(registry, path);
```

To iterate over the scene, see the [`Tree` component](#tree).

## Adding geometry to scene

### Default Physically Based Render (PBR)

Adds previously registered mesh geometry to the scene. This mesh will be rendered using PBR.

```c++
ui::Entity scene_object = ui::show_mesh(registry, mesh_entity);
```

Uses `DefaultShaders::PBR` shader.

See [Materials](#Materials) section to see how to control the appearance.


### Mesh visualizations
Adds a visualization of a mesh
([Jeremie's idea](https://git.corp.adobe.com/lagrange/Lagrange/issues/657)).

```c++
auto vertex_viz_entity = ui::show_vertex_attribute(registry, mesh_entity, attribute_name, glyph_type);
auto facet_viz_entity = ui::show_facet_attribute(registry, mesh_entity, attribute_name, glyph_type);
auto corner_viz_entity = ui::show_corner_attribute(registry, mesh_entity, attribute_name, glyph_type);
auto edge_viz_entity = ui::show_edge_attribute(registry, mesh_entity, attribute_name, glyph_type);
```

These functions will create a new scene object and render the supplied attribute using the selected glyph type.

#### `GlyphType::Surface`

Renders unshaded surface with color mapped from the supplied attribute. Supports attributes of dimension: 1, 2, 3, and 4.

*Normalization*: The attribute value is automatically remapped to (0,1) range. To change the range, use `ui::set_colormap_range`
*Colormapping*: By default, the attribute is interpreted as R, RG, RGB or RGBA value. To use different mapping, refer to [Colormaps](#Colormaps) section.

#### Colormaps

If the glyph or shader supports colormapping, use the following function to set the colormap:

To use on of the default colormaps:
```c++
ui::set_colormap(registry, entity, ui::generate_colormap(ui::colormap_magma))
```
Or generate your own
```c++
ui::set_colormap(registry, entity, ui::generate_colormap([](float t){
    return Color(
        //... function of t from 0 to 1
    );
}));
```

Default colormaps:
```
colormap_viridis
colormap_magma
colormap_plasma
colormap_inferno
colormap_turbo
colormap_coolwarm
```


<!--
Glyph Categories
- Color Output
- Geometry Output
- Texture Output

Glyph types:
-  `GlyphType::Surface`
   -  Dimension = 3
   -  Vertex/Facet/Corner/Edge
   -  Output = Color
-  `GlyphType::RGBA`
   -  Dimension = 4
   -  Vertex/Facet/Corner/Edge
   -  Output = Color
-  `GlyphType::Sphere`
   -  Dimension = 1
   -  Vertex
   -  Output = Geometry (position + size + Color)
-  `GlyphType::EigenEllipsoid`
   -  (Not a priority, only if we need tensor visualization)
   -  Dimension = 3x3
   -  Vertex/Facet
   -  Output = Geometry (frame + Color)
-  `GlyphType::Arrow`
   -  Dimension = 3
   -  Vertex/Facet/Corner/Edge
   -  (Possible options: normalization)
   -  Output = Geometry (position + Vector3 + Color)
-  `GlyphType::Colormap`
   -  Dimension = 1
   -  Vertex/Facet/Corner/Edge
   -  (Possible options: which colormap)
   -  Output = Color
-  `GlyphType::Parametrization`
   -  Dimension = 2
   -  (Vertex?)/Corner
   -  Output = Texture
-  `...`
-->

## Materials

Any entity with `MeshRender` component has a `Material` associated with it (`MeshRender::material`).

To get a reference to entity's material, use:

```c++
std::shared_ptr<Material> material_ptr = ui::get_material(r, entity_with_meshrender);
```

Similarly, you may set a new material:
```c++
ui::set_material(r, entity_with_mesh_render, std::make_shared<ui::Material>(r, DefaultShaders::PBR);
```

### Color/Texture Material Properties

You may set colors and textures of materials using the following API:

```c++
auto & material = *ui::get_material(r, entity_with_meshrender);

//Sets "property name" to a red color
material.set_color("property name", ui::Color(1,0,0));

//Sets "texture name" to texture loaded from file
material.set_texture("texture name", ui::load_texture("texture.jpg"));
```

#### PBRMaterial
For the default `PBRMaterial`, you may use aliases for the property names:
```c++

//Uniform rgba color
material.set_color(PBRMaterial::BaseColor, ui::Color(1,0,0,1));
//RGB(A) color/albedo texture
material.set_texture(PBRMaterial::BaseColor, ui::load_texture("color.jpg"));

//Normal texture (and texture only)
material.set_texture(PBRMaterial::Normal, ui::load_texture("normal.jpg"));

//Uniform roughness
material.set_float(PBRMaterial::Roughness, 0.75f);
//Roughness texture
material.set_texture(PBRMaterial::Roughness, ui::load_texture("metallic.jpg"));

//Uniform roughness
material.set_float(PBRMaterial::Metallic, 0.75f);
//Metallic texture
material.set_texture(PBRMaterial::Metallic, ui::load_texture("metallic.jpg"));

//Uniform opacity
material.set_float(PBRMaterial::Opacity, 1.0f);
//Opacity texture
material.set_texture(PBRMaterial::Opacity, ui::load_texture("opacity.jpg"));
```

### Rasterizer Properties

To control OpenGl properties, you may following syntax:

```c++
material.set_int(RasterizerOptions::PolygonMode, GL_LINE);
material.set_float(RasterizerOptions::PointSize, PointSize);
```

See `<lagrange/ui/Shader.h>` for a list of supported `RasterizerOptions`;

### Custom Shader Properties

You may set arbitrary `int` or `float` or `Color` or `Texture` to the material. It will be set as a shader uniform if it exists in the shader, otherwise there will be no effect.


## Common components
Entities can have several components that define their behavior. Here is a list of the common components used throughout Lagrange UI.

### `Name`
Subclassed `std::string`. Acts as a display name. Will be shown in UI if it exists, otherwise a generated name will be used. Does not have to be unique.

### `Transform`
Contains local and global transformations and a viewport transform.

```c++
// Translates entity one unit in X direction
ui::Transform & transform = registry.get<ui::Transform>(e);
transform.local = Eigen::Translation3f(1,0,0);
```

Global transformation is recomputed after each `Simulation` step. Only change the `local` transform.

### `Tree`
Defines scene tree relationship. Data is stored using `parent`, `first_child`, `previous_sibling` and `next_sibling` entity IDs.

Use helper functions to query or change the tree structure, do not change directly (unless you know what you're doing).
```c++
//Orphans entity and parents it under new_parent
ui::reparent(registry, entity, new_parent);

//Applies lambda to each direct child entity of parent
ui::foreach_child(registry, parent, [](Entity child){
    //...
});

//Applies lambda to each  child entity of parent, recursively
ui::foreach_child_recursive(registry, parent, [](Entity child){
    //...
});

//In-order traversal of scene tree
ui::iterate_inorder(registry, root, [](Entity current){
    //On Enter

    //Return true to continue to traverse children
    return true;
},[](Entity current){
    //On Exit
});

// See utils/treenode.h for more details
```

### `MeshGeometry`
Contains reference to geometry entity

```c++
MeshGeometry mg;
mg.entity = ..
```


### `Hovered` and `Selected`

These components acts as flags whether the entity is hovered or selected respectively.

Useful helper functions
```c++
bool is_selected(Registry &registry, Entity e);
bool is_hovered(Registry &registry, Entity e);
bool select(Registry& registry, Entity e);
bool deselect(Registry& registry, Entity e);
std::vector<Entity> collect_selected(const Registry& registry);
std::vector<Entity> collect_hovered(const Registry& registry);
//See `utils/selection.h` for details
```

### `Layer`

There are 256 layers an entity can belong to. The `Layer` component specifies which layers the entity belongs to. Entity can belong to several layers at once. There are several default layers:
`ui::DefaultLayers::Default` - everything belongs to it by default
`ui::DefaultLayers::Selection` - selected entities
`ui::DefaultLayers::Hover` - hovered entities

Default constructed `Layer` component belongs to `ui::DefaultLayers::Default`.

You can register your own layer by calling
```c++
ui::LayerIndex layer_index = ui::register_layer_name(r, "my layer name");
```

There are several utility functions for working with layers:
```c++
void add_to_layer(Registry&, Entity e, LayerIndex index);
void remove_from_layer(Registry&, Entity e, LayerIndex index);
bool is_in_layer(Registry&, Entity e, LayerIndex index);
bool is_in_any_layers(Registry&, Entity e, Layer layers_bitset);
bool is_visible_in(
    const Registry&,
    Entity e,
    const Layer& visible_layers,
    const Layer& hidden_layers);
```


### `UIPanel`
See [section](#User-Interface-Panels) below.

### `Viewport`
See [section](#Viewports) below.

## User Interface Panels

UI Panels are implemented also as entities. Panels have the `UIPanel` component. The `UIPanel` components describes the ImGui information (panel title, position, etc.).

To create a new UI panel:
```c++
auto panel_entity = ui::add_panel(registry, "Title of the panel",[](){
    // Do NOT call Imgui::Begin()/End()
    Imgui::Text("Hello world");
});
//or
auto panel_entity = ui::add_panel(registry, "Title of the panel", [](Registry &registry, Entity e){
    //Entity e is the panel_entity
});
```

Example of multiple instances of a same "type" of panel:

```c++

struct MyPanelState { int x = 0; }

auto panel_fn = [](Registry &registry, Entity e){
    auto & state = registry.get_or_emplace<MyPanelState>(e);
    ImGui::InputInt("x", &state.x);
};

auto panel0 = ui::add_panel(registry,"panel with x = 0",panel_fn)
registry.emplace<MyPanelState>(panel0, MyPanelState{0})

auto panel1 = ui::add_panel(registry,"panel with x = 1",panel_fn);
registry.emplace<MyPanelState>(panel1, MyPanelState{1})

```


## Viewports

Viewports are implemented as entities with `ViewportComponent` component. Those referenced in `ViewportPanel` are rendered to screen, otherwise they are rendered off-screen. There is always one **focused** `ViewportPanel` (identified by the context variable `FocusedViewportPanel`).

See `components/Viewport.h` and `utils/viewport.h` for utility functions related to viewport, viewport panels and cameras.

### Entity visibility

Each `ViewportComponent` has `visible_layers` and `hidden_layers` that control which entities can be renderer in this viewport (see [`Layer` component](#layer) for details).

The default viewport shows only `DefaultLayers::DefaultLayer`


### Multi viewport

Additional viewports can be created by calling
```c++
ui::Entity camera_entity = add_camera(ui::Registry &, ui::Camera camera);
// or use get_focused_camera_entity(ui::Registry &)  to reuse current camera

// Creates an offscreen viewport with the specified camera
ui::Entity viewport_entity = add_viewport(ui::Registry &, ui::Entity camera_entity)

// Creates a UI panel that shows the viewport
ui::Entity viewport_entity = add_viewport_panel(ui::Registry &, const std::string & name, ui::Entity viewport_entity);
```

---

## Entity Component System

For more information about the ECS architecture, see:
- [What you need to know about ECS](https://medium.com/ingeniouslysimple/entities-components-and-systems-89c31464240d) for quick overview
- [Overwatch Gameplay Architecture - GDC Talk](https://www.youtube.com/watch?v=W3aieHjyNvw) for a good example of usage and design considerations.
- [entt Crash Course](https://github.com/skypjack/entt/wiki/Crash-Course:-entity-component-system) for overview of the underlying `entt` library
- [ECS Back and Forth](https://skypjack.github.io/2019-06-25-ecs-baf-part-4/) for more details about ECS design, in particular hierarchies
- [Unity ECS documentation](https://docs.unity3d.com/Packages/com.unity.entities@0.1/manual/index.html) for Unity's version of ECS

### Registry
The `Viewer` uses a `Registry` (alias for `entt::registry`) to store all entities and their data. To manipulate entities and their components directly, use the object:
```c++
auto & registry = viewer.registry();
```
`Viewer` class exposes API that simplifies interaction with the `Registry`, e.g. `Viewer::show_mesh`.

### Entity
Unique identifier - it's just that. It's used to identify a unique "object" or "entity". Lagrange UI defines a `Entity` alias. Internally implemented as `std::uint32_t`.

To create a new entity, use:
```c++
Entity new_entity = registry.create();
```

To destroy:
```c++
registry.destroy(entity);
```

### Components
Any data that is attached to an `Entity`. Uniquely identified by template typename `<T>` and `Entity`.

Components **don't have logic, that means no code**. They only store data and implicitly define behavior. Ideally, the components should be `structs` with no functions. However, it may be beneficial to have setters/getters as member functions in some cases.


To attach a component of type `MyComponent` to an entity :
```c++
// When it doesn't exist
registry.emplace<MyComponent>(entity, MyComponent(42))

// When it might exist already
registry.emplace_or_replace<MyComponent>(entity, MyComponent(42))
```

To retrieve a component:
```c++
// If it exists already
MyComponent & c = registry.get<MyComponent>(entity);

// If you're not sure it exists
MyComponent * c = registry.try_get<MyComponent>(entity);
//or
if(registry.all_of<MyComponent>()){
    MyComponent& c = registry.get<MyComponent>(entity);
}
```

#### Tag Components

"Empty" components may be used to tag entities, e.g. `Selected`, `Hovered`, etc. These types however must have non-zero size:
```c++
struct Hidden {
    bool dummy;
}
```

### Systems

Systems are the logic of the application. They are defined as functions that iterate over entities that have specified components only.
For example, running this system:
```c++
registry.view<Velocity, Position>().each([](Entity e, Velocity & velocity, Transform & transform){
    transform.local = Eigen::Translation3f(velocity) * transform.local;
});
```
will iterate over all entities that have both `Velocity` and `Transform` and apply the velocity vector to the transform.


Lagrange UI defines `System` as alias to `std::function<void(Registry&)>`, that is, a function that does something with the `Registry`. Typically these will be defined as:
```c++
System my_system = [](Registry &w){
    w.view<Component1, Component2, ...>.each([](Entity e, Component1 & c1, Component2 & c2, ...){
        //
    });
};
```


### Context variables

Systems **do not have data**. However, it's often useful to have some state associated with a given system, e.g. for caching. Sometimes it's useful that this state be shared among several systems. Instead of storing this state in some single instance of a component, we can use *context* variables. These can be thought of as *singleton* components - only one instance of a `Type` can exist at a given time.

`InputState` is such a *singleton* component. At the beginning of the frame, it is filled with key/mouse information, including last mouse position, mouse delta, active keybinds, etc.:
```c++
void update_input_system(Registry & registry){
    InputState & input_state =  registry.ctx().emplace<InputState>();
    input_state.mouse.position = ...
    input_state.mouse.delta = ...
    input_state.keybinds.update(...);
}
```

It can then be used by any other system down the line:
```c++
void print_mouse_position(Registry & registry){
    const auto & input_state = registry.ctx().get<InputState>();

    lagrange::logger().info("Mouse position: {}", input_state.mouse_pos);
}
```

### Design considerations

Rules to follow when designing components and systems:
- Components have no functions, only data
- Systems have no data
- State associated with systems is stored as context variable (`registry.ctx().get<T>()`)

*TODO: const Systems / const views*



## Customizing Lagrange UI

TODO: add more details and examples. In the meantime, refer to files named `default_{}` to see how the UI registers the default types and functionality.

### Components

You may add any time of component using `registry.emplace<ComponentType>(entity)`. However to enable more advanced features, you may register the components in the UI:

`register_component<T>`
- enables reflection
- enables runtime add/clone/move of components

`register_component_widget<T>`
- defines ImGui code to render
- enables drag-and-drop


### Tools

*TBD*

`register_element_type<E>` (Object/Facet/Edge/Vertex/...)

`register_tool<E,T>` (Select/Translate/Rotate/Scale/...)

### Geometry

Lagrange meshes must be registered to work. By default, only the `TriangleMesh3Df` and `TriangleMesh3D` are registered.

`ui::register_mesh_type<MeshType>()`

### Rendering

#### Shader and Material properties

Material properties can be defined in the shader using the following syntax:


```glsl
#pragma property NAME "DISPLAY NAME" TYPE(DEFAULT VALUE AND/OR RANGE) [TAG1, TAG2]
```

For example:
```glsl
//Defines a 2D texture property with the default value of rgba(0.7,0.7,0.7,1) if no texture is bound
#pragma property material_base_color "Base Color" Texture2D(0.7,0.7,0.7,1)
//Defines a 2D texture property with the default value of red=0.4 if no texture is bound
#pragma property material_roughness "Roughness" Texture2D(0.4)
//Defines a 2D texture property with the default value of red=0.1 if no texture is bound
#pragma property material_metallic "Metallic" Texture2D(0.1)
//Defines a 2D texture property that is to be interpreted as normal texture
#pragma property material_normal "Normal" Texture2D [normal]
//Defines a float property, with the default value of 1 and range 0,1
#pragma property material_opacity "Opacity" float(1,0,1)
```

The pragmas are parsed whenever a shader is loaded and replaced with:
```glsl
uniform TYPE NAME = DEFAULT_VALUE
```
In case of `Texture2D`, these uniforms are generated:
```glsl
uniform sampler2D NAME;
uniform bool NAME_texture_bound = false;
uniform VEC_TYPE NAME_default_value = DEFAULT_VALUE;
```




## Examples

Refer to `modules/ui/examples`. Build Lagrange with `-DLAGRANGE_EXAMPLES=On`.
