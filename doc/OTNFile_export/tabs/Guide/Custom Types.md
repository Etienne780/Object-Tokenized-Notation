---
id: node_mu55p95xk2i6alcuiyw
name: Custom Types
---

# Custom Types

OTN supports serialization of arbitrary C++ structs through template specialization of `OTN::ToOTNDataType<T>`.

## Defining a specialization

Provide a `template<>` specialization in a header visible to both your write and read code. The function receives an `OTNObjectBuilder&` and a reference to your type `T`.

```cpp
struct Vector2 { 
  float x, y;
};

template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& v) {
    obj.SetObjectName("Vector2");
    obj.AddNames("x", "y");
    obj.AddData(v.x, v.y);
}
```

The builder operates in **two modes** transparently:

- **Serialization** (`T → OTNObject`): `AddData` captures the values from your struct.
- **Deserialization** (`OTNObject → T`): `AddData` writes values back into your struct.

You use the same function body for both directions in the simple case.

## Using a custom type

Once specialized, your type can be used anywhere a value is expected — as a column value, inside a vector, or nested within another object.

```cpp
Vector2 pos{ 10.0f, 20.0f };

OTN::OTNObject obj{ "Entities" };
obj.SetNames("id", "position")
   .AddDataRow(1, pos);
```

Reading back:

```cpp
auto maybePos = obj->TryGetValue<Vector2>(0, "position");
if (maybePos) {
    float x = maybePos->x;
}
```

## Separating save and load logic

Use `IsBuildingOTNObject()` when the save and load paths need to differ — for example, when packing data into a compact representation on save and unpacking it on load.

```cpp
template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& e) {
    obj.SetObjectName("Vector2");
    obj.AddNames("coords");

    if (obj.IsBuildingOTNObject()) {
        // Serialization: pack x and y into a float list
        obj.AddData(std::vector<float>{ e.x, e.y });
    } else {
        // Deserialization: unpack from the list
        std::vector<float> tmp;
        obj.AddData(tmp);
        if (tmp.size() >= 2) {
            e.x = tmp[0];
            e.y = tmp[1];
        }
    }
}
```

## Rules for custom types

- Your type **must have a default constructor** (used during deserialization).
- The order in which you call `AddData` determines the column order in the file. **Changing this order is a breaking change** — existing files will no longer deserialize correctly.
- `SetObjectName` is required. The name is used as the type identifier in the OTN file.
- `AddNames` only matters during serialization. During deserialization, data is consumed positionally by `AddData` calls.

## Nested custom types

Custom types can contain other custom types. Each nested type will be serialized as a sub-object and written to the file before its parent.

```cpp
struct Transform {
    Vector2 position;
    float rotation;
};

template<>
inline void OTN::ToOTNDataType<Transform>(OTN::OTNObjectBuilder& obj, Transform& t) {
    obj.SetObjectName("Transform");
    obj.AddNames("position", "rotation");
    obj.AddData(t.position, t.rotation);
}
```

## Using custom types in vectors

```cpp
std::vector<Vector2> path = { {0,0}, {1,2}, {3,4} };

OTN::OTNObject route{ "Route" };
route.SetNames("waypoints")
     .AddDataRow(path);
```

Reading back:

```cpp
auto waypoints = obj->TryGetValue<std::vector<Vector2>>(0, "waypoints");
```
