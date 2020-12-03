#include "myVector.h"
#include <stdlib.h>

int vector_init(struct int_vector* vector)
{
	vector->container = malloc(INITIAL_ALLOC * sizeof(int));
	vector->size = 0;
	vector->allocated = INITIAL_ALLOC;

	return 0;
}

int vector_push_back(struct int_vector* vector, int value);
{
	if(vector->size == vector->allocated)
	{
		vector->container = realloc(vector->container, vector->allocated * 2 * sizeof(int));
		vector->allocated *= 2;
	}

	vector->container[vector->size] = value;
	vector->size ++;

	return 0;

}
int vector_remove(struct int_vector* vector, int value)
{
	int i = 0;

	while(i < vector->size)
	{
		if(vector->container[i] == value)
		{
			for(int j = i; j < vector->size - 1; j++)
			{
				vector->container[j] = vector->container[j + 1];
			}

			vector->size --;
		}
		
		i++;
	}

	return 0;
}

void vector_free(struct int_vector* vector) 
{
	free(vector->container);
}