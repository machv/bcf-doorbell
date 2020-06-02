#include <list.h>

void list_init(list_t* list) {
	list->head = NULL;
	list->length = 0;
}

// insert new node at the top
void list_add_first(list_t* list, int when, bool is_long) {
	// allocate node
	list_node_t* new_node = (list_node_t*)malloc(sizeof(list_node_t));

	if (new_node == NULL) {
		return;
	}

	// put in the data
	new_node->when = when;
	new_node->is_long = is_long;

	if (list->head == NULL) {
		new_node->next = new_node->prev = new_node;
		list->head = new_node;
		list->length += 1;
		return;
	}

	/* Find last node */
	list_node_t* last = list->head->prev;

	// setting up previous and next of new node 
	new_node->next = list->head;
	new_node->prev = last;

	// Update next and previous pointers of start 
	// and last. 
	last->next = list->head->prev = new_node;

	// Update start pointer 
	list->head = new_node;
	list->length += 1;
}

void list_add_last(list_t* list, int when, bool is_long) {
	// allocate node
	list_node_t* new_node = (list_node_t*)malloc(sizeof(list_node_t));

	if (new_node == NULL) {
		return;
	}

	// put in the data
	new_node->when = when;
	new_node->is_long = is_long;

	if (list->head == NULL) {
		new_node->next = new_node->prev = new_node;
		list->head = new_node;
		list->length += 1;
		return;
	}

	/* Find last node */
	list_node_t* last = list->head->prev;

	// Start is going to be next of new_node 
	new_node->next = list->head;

	// Make new node previous of start 
	list->head->prev = new_node;

	// Make last preivous of new node 
	new_node->prev = last;

	// Make new node next of old last 
	last->next = new_node;

	list->length += 1;
}

void list_delete_node(list_t* list, list_node_t* del) {
	if (list->head == NULL || del == NULL)
		return;

	if (del->next == del && del->prev == del && del == list->head) {
		list->head = NULL;
		list->length = 0;
		free(del);
		return;

	}

	list_node_t* prev = del->prev;

	// If list has more than one node, 
	// check if it is the first node 
	if (del == list->head) {
		// Move prev_1 to last node 
		prev = list->head->prev;

		// Move start ahead 
		list->head = list->head->next;

		// Adjust the pointers of prev_1 and start node 
		prev->next = list->head;
		list->head->prev = prev;
		free(del);
	}
	// check if it is the last node 
	else if (del->next == list->head) {
		// Adjust the pointers of prev_1 and start node 
		prev->next = list->head;
		list->head->prev = prev;
		free(del);
	}
	else {
		// create new pointer, points to next of curr node 
		list_node_t* temp = del->next;

		// Adjust the pointers of prev_1 and temp node 
		prev->next = temp;
		temp->prev = prev;
		free(del);
	}

	list->length -= 1;
}

void list_delete_first(list_t* list) {
	if (list->head != NULL)
		list_delete_node(list, list->head);
}

void list_delete_last(list_t* list) {
	if (list->head != NULL)
		list_delete_node(list, list->head->prev);
}

void list_clear(list_t* list) {
	if (list == NULL)
		return;

	if (list->head == NULL)
		return;

	int len = list->length;
	list_node_t* next;
	list_node_t* curr = list->head;

	while (len--) {
		next = curr->next;

		list_delete_node(list, curr);

		curr = next;
	}
}

void list_print(list_t* list)
{
	if(list == NULL || list->head == NULL)
		return;

	bc_log_info("List (length = %d): ", list->length);

    list_node_t* temp = list->head;
    
	do { 
		bc_log_info("  [%llu] %c ", temp->when, (temp->is_long ? '-' : '.'));
		temp = temp->next; 
	} while (temp != list->head); 
}

/// pattern = list of expected presses
/// input = actual history of presses
/// timespan = expected time span of all presses in input
bool list_compare(list_t* pattern, list_t* input, bc_tick_t timespan) {
	if (pattern == NULL || input == NULL) {
		//bc_log_info("MORSE: [COMPARE] Empty list");
		return false;
	}
	
	if (pattern->length > input->length) {
		//bc_log_info("MORSE: [COMPARE] Wrong length");
		return false;
	}

	list_node_t* iterator_pattern = pattern->head;
	list_node_t* iterator_input = input->head;
	bc_tick_t last_time = input->head->when;

	while (iterator_pattern->next != pattern->head)
	{
		// now compare if reach node is of the same type
		if (iterator_pattern->is_long != iterator_input->is_long) {
			//bc_log_info("MORSE: [COMPARE] Button type differs");
			return false;
		}
		
		// if current press is older then <timespan> milliseconds from the initial, fail comparison
		if ((iterator_input->when - last_time) > timespan) {
			//bc_log_info("MORSE: [COMPARE] Out of time range");
			return false;
		}

		iterator_pattern = iterator_pattern->next;
		iterator_input = iterator_input->next;
	}

	return true;
}
