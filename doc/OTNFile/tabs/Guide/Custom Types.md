---
id: node_mu55p95xk2i6alcuiyw
name: Custom Types
---

# Custom Types

Your own structs can be used like any other value. You only have to specialize `OTN::ToOTNDataType<T>` once. The same function is used for saving and for loading.

## Defining a type

```cpp
struct Vector2 {
    float x = 0;
    float y = 0;
};

template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& v) {
    obj.SetObjectName("Vector2");
    obj.AddNames("x", "y");
    obj.AddData(v.x, v.y);
}
```

Put the specialization into a header that is included everywhere the type is written or read.

- `SetObjectName` sets the name of the object that stores all values of this type.
- `AddNames` sets the column names.
- `AddData` reads the members when saving and writes them when loading.

`AddNameData("x", v.x)` combines `AddNames` and `AddData` for one column.

## Using a type

```cpp
OTN::OTNObject entities{ "Entities" };
entities.SetNames("id", "position", "path")
        .AddDataRow(1, Vector2{ 10, 20 }, std::vector<Vector2>{ { 0, 0 }, { 1, 2 } });
```

Every value becomes one row of the object `Vector2`. The column stores the row index:

```otn
@version: 1;

@object: {
	Vector2[3] {
		float/x, float/y
	};
	10, 20;
	0, 0;
	1, 2;
	Entities[1] {
		int/id, Ref<Vector2>/position, Ref<Vector2>[]/path
	};
	1, 0, [1, 2];
};
```

Reading back:

```cpp
auto position = entities->TryGetValue<Vector2>(0, "position");
auto path     = entities->TryGetValue<std::vector<Vector2>>(0, "path");
```

Custom types work with `OTNWriter`, `OTNStreamWriter` (also in `WriteRow`), `OTNReader` and `OTNStreamReader`.

## Nested types

A type can contain other custom types, lists and strings:

```cpp
struct Transform {
    Vector2 position;
    float rotation = 0;
    std::vector<std::string> tags;
};

template<>
inline void OTN::ToOTNDataType<Transform>(OTN::OTNObjectBuilder& obj, Transform& t) {
    obj.SetObjectName("Transform");
    obj.AddNames("position", "rotation", "tags");
    obj.AddData(t.position, t.rotation, t.tags);
}
```

## Different code for saving and loading

`IsBuildingOTNObject()` returns `true` while saving. Use it when the stored form differs from the struct:

```cpp
template<>
inline void OTN::ToOTNDataType<Vector2>(OTN::OTNObjectBuilder& obj, Vector2& v) {
    obj.SetObjectName("Vector2");
    obj.AddNames("coords");

    if (obj.IsBuildingOTNObject()) {
        // saving: store x and y as one list
        obj.AddData(std::vector<float>{ v.x, v.y });
    } else {
        // loading: unpack the list
        std::vector<float> coords;
        obj.AddData(coords);
        if (coords.size() >= 2) {
            v.x = coords[0];
            v.y = coords[1];
        }
    }
}
```

## Rules

- The type needs a **default constructor**, it is created before loading.
- When loading, values are assigned **by position** in the order of the `AddData` calls, not by name. Changing the order or the types of `AddData` makes existing files unreadable.
- All values of a type end up in one object. Use the same `SetObjectName` and the same columns for every value of the type.
- The object name and column names must not contain OTN syntax characters and must not start with a digit.
