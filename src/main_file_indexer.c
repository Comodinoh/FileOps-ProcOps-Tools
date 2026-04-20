#include <db.h>

uint32_t idx_generate_snapshot(const char* path) {
    return 1;
}

int main(void) {
    db_connection connection = {0};
    db_column columns[3] = {
        {.kind = DB_STRING, .name = "TestName"},
        {.kind = DB_INT, .name = "perms"},
        {.kind = DB_DOUBLE, .name = "acc"}
    };
    db_schema schema = {"IDX", columns, sizeof(columns)/sizeof(columns[0])};
    db_open_connection(&connection, "idx.db", &schema, &idx_generate_snapshot);
}
