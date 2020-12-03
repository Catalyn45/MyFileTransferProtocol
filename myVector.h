
#define INITIAL_ALLOC 10

struct int_vector
{
	int* container;
	int size;
	int allocated;
};

int vector_init(struct int_vector* vector);
int vector_push_back(struct int_vector* vector, int value);
int vector_remove(struct int_vector* vector, int value);
void vector_free(struct int_vector*);