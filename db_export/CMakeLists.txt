add_executable(db_export
    main.cpp
    schema_introspect.cpp
    schema_introspect.h
    table_copier.cpp
    table_copier.h
)
target_link_libraries(db_export PRIVATE Qt6::Core Qt6::Sql)
