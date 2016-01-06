local value = 10
mount.register("a.mount.path.Entity", value);
print("IN REGISTRY: " + import("a.mount.path.Entity"));
mount.register("a.mount.path.Entity", value, true);

