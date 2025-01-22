#include <stdio.h>
#include <stdlib.h>

typedef struct table_element {
    SOCKET* key;
    char* username;
} table_elem;

typedef struct h_table {
    table_elem *table;
    size_t table_size;
    size_t num_of_elems;
}  table_t;

char* table_search(table_t *table, size_t table_size, SOCKET* key) {
    for (size_t i = 0; i < table_size; i++)
    {
    if(table->table[i].key == key)
        {
            return table->table[i].username;
        }            
    }
    return NULL;
}

void append_element(table_t *table, table_elem appending_elem) {
    if(table->table_size <= table->num_of_elems) {
        table = realloc(table, (table->table_size) * 2);
        table->table_size = table->table_size * 2; 
        table->table[(table->num_of_elems) + 1] = appending_elem;
        table->num_of_elems = table->num_of_elems + 1;  
        return;
    }

// else
table->table[(table->num_of_elems) + 1] = appending_elem;
table->num_of_elems = table->num_of_elems + 1;  
return;
}

// TODO: remove element