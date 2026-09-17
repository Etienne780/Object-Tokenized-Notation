---
id: node_mu55p95x1j1aoqjvokje3
name: Reading Data
---

# Reading Data

Use `OTNReader` to load a `.otn` file into memory. All objects are fully parsed and accessible via name lookup.

## Opening a file

```cpp
OTN::OTNReader reader;
if (!reader.ReadFile("data.otn")) {
    std::cerr << reader.GetError() << std::endl;
    return;
}
```

You can also parse from a string:

```cpp
std::string otnText = /* ... */;
if (!reader.ReadString(otnText)) { /* ... */ }
```

## Retrieving an object

```cpp
auto obj = reader.TryGetObject("Items");
if (!obj) {
    // object not found
}
```

`TryGetObject` returns `std::optional<OTNObject>`, so it is safe even if the name doesn't exist in the file.

## Reading values

There are three ways to read a value from a row, depending on how much safety you need.

**Throwing access (use when the data must exist):**

```cpp
int id = obj->GetValue<int>(0, "id");
std::string name = obj->GetValue<std::string>(0, "name");
float val = obj->GetValue<float>(0, "value");
```

**With a fallback default (safe, no exception):**

```cpp
int   id  = obj->GetValue<int>(0, "id", -1);
float val = obj->GetValue<float>(0, "value", 0.0f);
```

**As `std::optional` (fully safe):**

```cpp
std::optional<int>         id   = obj->TryGetValue<int>(0, "id");
std::optional<std::string> name = obj->TryGetValue<std::string>(0, "name");
```

All three variants accept either a column name (`"id"`) or a column index (`0`).

## Iterating all rows

```cpp
for (size_t row = 0; row < obj->GetRowCount(); ++row) {
    int id = obj->GetValue<int>(row, "id", -1);    
    auto name = obj->TryGetValue<std::string>(row, "name");
}
```

## Reading all objects at once

```cpp
const auto& allObjects = reader.GetObjects();
for (const auto& [name, object] : allObjects) {
    // process each object
}
```

## Reading lists / arrays

If a column was saved with a vector value (e.g. `std::vector<int>`), read it back the same way:

```cpp
auto tags = obj->TryGetValue<std::vector<int>>(0, "tags");
if (tags) {
    for (int t : *tags) { /* ... */ }
}
```

## Objects with multiple rows become lists

When an `OTNObject` is stored as a nested value inside another object and that inner object has **more than one row**, it is stored in the file as a list of integer row indices. When reading back, such a column must be read as `std::vector<OTNObject>` or `std::vector<YourCustomType>`, not as a single value.

```cpp
// Writing — "inventory" column contains multiple Item rows
OTN::OTNObject items{ "Item" };
items.SetNames("name", "qty")
     .AddDataRow("Sword", 1)
     .AddDataRow("Potion", 5);

OTN::OTNObject player{ "Player" };
player.SetNames("name", "inventory")
      .AddDataRow("Hero", items); // items has 2 rows → becomes a list

// Reading
auto items = obj->TryGetValue<std::vector<OTNObject>>(0, "inventory");
```

A nested object with exactly one row is read back as a single value. The moment it has two or more rows it becomes a list in the file, and you must read it as a list.
