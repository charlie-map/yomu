#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "yomu.h"

int is_range(char c);
char **split_string(char *full_string, char delimeter, int *arr_len, char *extra, ...);

void *resize_array(void *array, int *max_size, int current_index, size_t singleton_size) {
	while (current_index >= *max_size) {
		*max_size *= 2;

		array = realloc(array, singleton_size * *max_size);
	}

	return array;
}

struct Yomu {
	char *tag;

	int data_index, *max_data;
	char *data; // for a singular type (like a char *)

	int *max_attr_tag, attr_tag_index;
	char **attribute; // holds any attributes for this tag
	// the attribute name is always an even position and
	// the attribute itself is odd

	int children_data_pos; // where the children occur within the
		// overall scheme of this token
		// ** mainly for token_read_all_data() to ensure the data
		// is read from the correct position
	int *max_children, children_index;
	struct Yomu **children; // for nest children tags

	struct Yomu *parent;
};

yomu_t *create_token(char *tag, yomu_t *parent) {
	yomu_t *new_token = malloc(sizeof(yomu_t));

	new_token->tag = tag;
	new_token->data = malloc(sizeof(char) * 8);
	memset(new_token->data, '\0', sizeof(char) * 8);
	new_token->max_data = malloc(sizeof(int));
	*new_token->max_data = 8;
	new_token->data_index = 0;

	new_token->max_attr_tag = malloc(sizeof(int));
	*new_token->max_attr_tag = 8;
	new_token->attr_tag_index = 0;
	new_token->attribute = malloc(sizeof(char *) * *new_token->max_attr_tag);

	new_token->max_children = malloc(sizeof(int));
	*new_token->max_children = 8;
	new_token->children_index = 0;
	new_token->children = malloc(sizeof(yomu_t *) * *new_token->max_children);

	new_token->parent = parent;

	return new_token;
}

int add_token_rolling_data(yomu_t *token, char data) {
	token->data[token->data_index] = data;

	token->data_index++;

	token->data = resize_array(token->data, token->max_data, token->data_index, sizeof(char));
	token->data[token->data_index] = '\0';

	return 0;
}

int update_token_data(yomu_t *curr_token, char *new_data, int *new_data_len) {
	// realloc data to fit new_data_len:
	*curr_token->max_data = *new_data_len;
	curr_token->data = realloc(curr_token->data, sizeof(char) * *new_data_len);

	strcpy(curr_token->data, new_data);

	return 0;
}

int add_token_attribute(yomu_t *token, char *tag, char *attribute) {
	token->attribute[token->attr_tag_index++] = tag;
	token->attribute[token->attr_tag_index++] = attribute;

	// check for resize
	token->attribute = resize_array(token->attribute, token->max_attr_tag, token->attr_tag_index, sizeof(char *));

	return 0;
}

char *token_attr(yomu_t *token, char *attr) {
	// search for attr
	int read_attr;
	for (read_attr = 0; read_attr < token->attr_tag_index; read_attr++) {
		if (strcmp(token->attribute[read_attr], attr) == 0)
			return token->attribute[read_attr + 1];
	}

	return NULL;
}

int set_attr(yomu_t *y, char *attr, char *new_val) {
	// search for attr
	int read_attr;
	for (read_attr = 0; read_attr < y->attr_tag_index; read_attr++) {
		if (strcmp(y->attribute[read_attr], attr) == 0)
			break;
	}

	char *new_new_val = malloc(sizeof(char) * strlen(new_val));
	strcpy(new_new_val, new_val);

	// need to add new attribute entirely
	if (read_attr == y->attr_tag_index) {
		char *new_attr = malloc(sizeof(char) * strlen(attr));
		strcpy(new_attr, attr);

		add_token_attribute(y, new_attr, new_new_val);

		return 0;
	}

	// otherwise just change value:
	free(y->attribute[read_attr + 1]);
	y->attribute[read_attr + 1] = new_new_val;

	return 0;
}

int add_token_children(yomu_t *token_parent, yomu_t *child) {
	token_parent->children[token_parent->children_index] = child;

	token_parent->children_index++;

	// check for resize
	token_parent->children = resize_array(token_parent->children, token_parent->max_children, token_parent->children_index, sizeof(yomu_t *));

	return 0;
}

yomu_t **token_children(yomu_t *parent) {
	return parent->children;
}

yomu_t *grab_token_children(yomu_t *parent) {
	if (parent->children_index == 0)
		return NULL;

	return parent->children[parent->children_index - 1];
}

yomu_t *grab_token_parent(yomu_t *curr_token) {
	return curr_token->parent;
}

char *data_at_token(yomu_t *curr_token) {
	return curr_token->data;
}

yomu_t **grab_tokens_by_match_helper(yomu_t *start_token, int (*is_match)(yomu_t *, char *), char *search, yomu_t **curr_found_tokens, int *found_token_length, int index_found_token, int recur) {
	for (int check_children = 0; check_children < start_token->children_index; check_children++) {
		// compare:
		if (is_match(start_token->children[check_children], search)) {
			curr_found_tokens[index_found_token] = start_token->children[check_children];
			index_found_token++;

			curr_found_tokens = resize_array(curr_found_tokens, found_token_length, index_found_token, sizeof(yomu_t *));
		}
	}

	// otherwise check children
	for (int bfs_children = 0; bfs_children < recur ? start_token->children_index : 0; bfs_children++) {
		curr_found_tokens = grab_tokens_by_match_helper(start_token->children[bfs_children], is_match, search, curr_found_tokens, found_token_length, index_found_token, recur);
	}

	return curr_found_tokens;
}

yomu_t **grab_tokens_by_match(yomu_t *start_token, int (*match)(yomu_t *, char *), char *search, int *length, int depth) {
	*length = 8;
	yomu_t **tokens = malloc(sizeof(yomu_t *) * *length);

	tokens = grab_tokens_by_match_helper(start_token, match, search, tokens, length, 0, depth);
	return tokens;
}

// DFS for first occurrence of a match
yomu_t *grab_token_by_match(yomu_t *y, int (*match)(yomu_t *, char *), char *search, int direction) {
	// search through current y to find a search term
	// start pos depends on direction:
	// 0 for starting at 0 and going to children_index
	// 1 for starting at childrend_index and going to 0
	for (int find_match = direction ? y->children_index - 1 : 0; direction ? find_match >= 0 : find_match < y->children_index; find_match += direction ? -1 : 1) {
		if (match(y->children[find_match], search))
			return y->children[find_match];

		// check children
		yomu_t *test_children = grab_token_by_match(y->children[find_match], match, search, direction);
		if (test_children)
			return test_children;
	}

	return NULL;
}

int destroy_token(yomu_t *curr_token) {
	free(curr_token->tag);
	free(curr_token->data);

	// free attributes
	for (int free_attribute = 0; free_attribute < curr_token->attr_tag_index; free_attribute++)
		free(curr_token->attribute[free_attribute]);
	free(curr_token->attribute);

	free(curr_token->max_data);
	free(curr_token->max_attr_tag);
	free(curr_token->max_children);

	// free children
	for (int free_child = 0; free_child < curr_token->children_index; free_child++)
		destroy_token(curr_token->children[free_child]);
	free(curr_token->children);

	free(curr_token);

	return 0;
}

int yomu_tagMETA(yomu_t *search_token, char *search_tag, char ***found_tag, int *max_tag, int *tag_index) {
	// check if current tree position has correct tag
	if (strcmp(search_token->tag, search_tag) == 0) {
		(*found_tag)[(*tag_index)++] = search_token->data;

		*found_tag = (char **) resize_array(*found_tag, max_tag, *tag_index, sizeof(char *));
	}

	// check children
	for (int check_child = 0; check_child < search_token->children_index; check_child++) {
		yomu_tagMETA(search_token->children[check_child], search_tag, found_tag, max_tag, tag_index);
	}

	return 0;
}

char **token_get_tag_data(yomu_t *search_token, char *tag_name, int *max_tag) {
	// setup char ** array
	*max_tag = 8;
	char **found_tag = malloc(sizeof(char *) * *max_tag);
	int *tag_index = (int *) malloc(sizeof(int));
	*tag_index = 0;

	yomu_tagMETA(search_token, tag_name, &found_tag, max_tag, tag_index);

	// move value into max tag (which becomes the index for the return value)
	*max_tag = *tag_index;
	free(tag_index);

	return found_tag;
}

char *resize_full_data(char *full_data, int *data_max, int data_index) {
	while (data_index >= *data_max) {
		*data_max *= 2;
		full_data = realloc(full_data, sizeof(char) * *data_max);
	}

	return full_data;
}

int token_read_all_data_helper(yomu_t *search_token, char **full_data, int *data_max, int data_index, void *block_tag, void *(*is_blocked)(void *, char *), int currently_blocked, int recur) {
	// update reads specifically for %s's first to place sub tabs into the correct places
	int add_from_child = 0;
	
	for (int read_token_data = 0; read_token_data < search_token->data_index; read_token_data++) {
		if (search_token->data[read_token_data] == '<' && (read_token_data < search_token->data_index - 1 &&
			search_token->data[read_token_data + 1] == '>')) {
			if (recur) {
				data_index = token_read_all_data_helper(search_token->children[add_from_child], full_data, data_max, data_index, block_tag, is_blocked,
					block_tag && is_blocked(block_tag, search_token->children[add_from_child]->tag), recur);

				// move to next child
				add_from_child++;
			}

			// skip other process
			read_token_data++;
			continue;
		}

		// skip if the tag is blocked
		if (currently_blocked)
			continue;

		// check full_data has enough space
		*full_data = resize_full_data(*full_data, data_max, data_index + 1);

		// add next character
		(*full_data)[data_index] = search_token->data[read_token_data];
		data_index++;
	}

	return data_index;
}

// go through the entire sub tree and create a char * of all data values
char *token_read_all_data(yomu_t *search_token, int *data_max, void *block_tag, void *(*is_blocked)(void *, char *), int recur) {
	int data_index = 0;
	*data_max = 8;
	char **full_data = malloc(sizeof(char *));
	*full_data = malloc(sizeof(char) * *data_max);

	data_index = token_read_all_data_helper(search_token, full_data, data_max, 0, block_tag, is_blocked, 0, recur);

	(*full_data)[data_index] = 0;

	*data_max = data_index;

	char *pull_data = *full_data;
	free(full_data);
	return pull_data;
}

char *read_token(yomu_t *search_token, char read_type) {
	return "";
}

int update_token(yomu_t *search_token, char *data) {
	return 0;
}

int has_attr_value(char **attribute, int attr_len, char *attr, char *attr_value) {
	int attr_pos;
	for (attr_pos = 0; attr_pos < attr_len; attr_pos += 2) {
		if (strcmp(attribute[attr_pos], attr) == 0)
			break;
	}

	if (attr_pos == attr_len)
		return 0;

	attr_pos++;

	int *classlist_len = malloc(sizeof(int));
	char **classlist = split_string(attribute[attr_pos], ' ', classlist_len, "-c");

	int found_class = 0;
	for (int check_classlist = 0; check_classlist < *classlist_len; check_classlist++) {
		if (strcmp(classlist[check_classlist], attr_value) == 0)
			found_class = 1;

		free(classlist[check_classlist]);
	}

	free(classlist);
	free(classlist_len);

	return found_class;
}

int token_has_classname(yomu_t *token, char *classname) {
	return has_attr_value(token->attribute, token->attr_tag_index, "class", classname);
}

// takes current search token and searches for the attribute `class=classname`
// will return first occurrence of a token that has that classname
yomu_t *grab_token_by_classname(yomu_t *start_token, char *classname) {
	// if it doesn't exists, return NULL
	for (int check_children = 0; check_children < start_token->children_index; check_children++) {
		// compare tag:
		if (has_attr_value(start_token->children[check_children]->attribute,
			start_token->children[check_children]->attr_tag_index, "class", classname))
			return start_token->children[check_children];
	}

	// otherwise check children
	for (int bfs_children = 0; bfs_children < start_token->children_index; bfs_children++) {
		yomu_t *check_children_token = grab_token_by_classname(start_token->children[bfs_children], classname);

		if (check_children_token)
			return check_children_token;
	}

	return NULL;
}

int read_main_tag(char **main_tag, char *curr_line, int search_tag) {
	*main_tag = malloc(sizeof(char) * 8);
	int max_main_tag = 8, main_tag_index = 0;

	search_tag++;

	while (curr_line[search_tag] != '>' &&
		   curr_line[search_tag] != '\n' &&
		   curr_line[search_tag] != ' ') {

		(*main_tag)[main_tag_index] = curr_line[search_tag];

		search_tag++;
		main_tag_index++;

		*main_tag = resize_array(*main_tag, &max_main_tag, main_tag_index, sizeof(char));

		(*main_tag)[main_tag_index] = '\0';
	}

	return search_tag;
}

int read_newline(char **curr_line, size_t *buffer_size, FILE *fp, char *str_read) {
	if (fp)
		return getdelim(curr_line, buffer_size, 10, fp);

	// otherwise search through str_read for a newline
	int str_read_index = 0;

	while (str_read[0] != '\0') {
		if (str_read[0] == '\n')
			break;

		(*curr_line)[str_read_index] = str_read[0];

		str_read += sizeof(char);
		str_read_index++;

		// check for resize
		if ((str_read_index + 1) * sizeof(char) == *buffer_size) {
			// increase
			*buffer_size *= 2;
			*curr_line = realloc(*curr_line, *buffer_size);
		}

		(*curr_line)[str_read_index] = '\0';
	}

	if (str_read[0] == '\0' && str_read_index == 0)
		return -1;

	(*curr_line)[str_read_index] = '\n';
	(*curr_line)[str_read_index + 1] = '\0';

	return str_read_index + (str_read[0] != '\0');
}

int find_close_tag(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_close) {
	while((*curr_line)[search_close] != '>') {
		if ((*curr_line)[search_close] == '\n') {
			search_close = 0;

			if (!file)
				str_read += (search_close + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		search_close++;
	}

	return search_close;
}

char *read_close_tag(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_close) {
	int *max_close_tag = malloc(sizeof(int)), close_tag_index = 0;
	*max_close_tag = 8;
	char *close_tag = malloc(sizeof(char) * 8);

	while((*curr_line)[search_close] != '>') {
		if ((*curr_line)[search_close] == '\n') {
			search_close = 0;

			if (!file)
				str_read += (search_close + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		close_tag[close_tag_index++] = (*curr_line)[search_close];
		close_tag = resize_array(close_tag, max_close_tag, close_tag_index, sizeof(char));
		close_tag[close_tag_index] = '\0';

		search_close++;
	}

	free(max_close_tag);

	return close_tag;
}

typedef struct TagRead {
	char *update_str_read;
	int new_search_token;
	int type; // 0 for normal (<div></div>), 1 for closure (<img/>), 2 for comment (<!---->)
} tag_reader;

tag_reader find_end_comment(FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_token) {

	tag_reader tag_read = { .new_search_token = search_token, .type = 2 };

	while (1) {
		if ((*curr_line)[search_token] == '-' && (*curr_line)[search_token + 1] == '-' && (*curr_line)[search_token + 2] == '>')
			break;

		if ((int) (*curr_line)[search_token] == 10) {
			search_token = 0;

			if (!file)
				str_read += (search_token + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			continue;
		}

		search_token++;
	}

	tag_read.new_search_token = search_token + 2;
	tag_read.update_str_read = str_read;

	return tag_read;
}

// builds a new tree and adds it as a child of parent_tree
tag_reader read_tag(yomu_t *parent_tree, FILE *file, char *str_read, char **curr_line, size_t *buffer_size, int search_token) {
	// check for comment:
	if ((*curr_line)[search_token + 1] == '!' && (*curr_line)[search_token + 2] == '-' && (*curr_line)[search_token + 3] == '-')
		return find_end_comment(file, str_read, curr_line, buffer_size, search_token);

	char *main_tag;
	search_token = read_main_tag(&main_tag, *curr_line, search_token);

	yomu_t *new_tree = create_token(main_tag, parent_tree);

	int read_tag = 1; // choose whether to add to attr_tag_name
					  // or attr_tag_value: 1 to add to attr_tag_name
					  // 0 to add to attr_tag_value
	int start_attr_value = 0; // checks if reading the attribute has begun
	/*
		primarily for when reading the attribute value:
		tag="hello"

		only after the first " is seen should reading the value occur
	*/

	char *attr_tag_name = malloc(sizeof(char) * 8);
	int max_attr_tag_name = 8, attr_tag_name_index = 0;
	memset(attr_tag_name, '\0', 8);
	char *attr_tag_value = malloc(sizeof(char) * 8);
	int max_attr_tag_value = 8, attr_tag_value_index = 0;
	memset(attr_tag_value, '\0', 8);

	tag_reader tag_read;

	// searching for attributes
	while ((*curr_line)[search_token] != '>') {

		if ((*curr_line)[search_token] == '\n') {
			if (!file)
				str_read += (search_token + 1) * sizeof(char);
			read_newline(curr_line, buffer_size, file, str_read);

			search_token = 0;
		}

		// skip not wanted characters
		if ((*curr_line)[search_token] == ' ' || (*curr_line)[search_token] == '\t') {
			search_token++;
			continue;
		}

		if (read_tag) {
			if ((*curr_line)[search_token] == '=') { // switch to reading value
				read_tag = 0;

				attr_tag_value_index = 0;

				search_token++;
				continue;
			} else if (attr_tag_name_index > 0 && (*curr_line)[search_token - 1] == ' ') {
				// this means there was no attr value, so add this
				// with a NULL value and move into the next one
				char *tag_name = malloc(sizeof(char) * (attr_tag_name_index + 1));
				strcpy(tag_name, attr_tag_name);

				add_token_attribute(new_tree, tag_name, NULL);

				attr_tag_name_index = 0;
				continue;
			}

			attr_tag_name[attr_tag_name_index++] = (*curr_line)[search_token];

			attr_tag_name = resize_array(attr_tag_name, &max_attr_tag_name, attr_tag_name_index, sizeof(char));
			attr_tag_name[attr_tag_name_index] = '\0';
		} else {
			if ((*curr_line)[search_token] == '"' || (*curr_line)[search_token] == '\'') {
				if (start_attr_value) {
					// add to the token
					char *tag_name = malloc(sizeof(char) * (attr_tag_name_index + 1));
					strcpy(tag_name, attr_tag_name);

					char *attr = malloc(sizeof(char) * (attr_tag_value_index + 1));
					strcpy(attr, attr_tag_value);
					add_token_attribute(new_tree, tag_name, attr);

					attr_tag_name_index = 0;

					read_tag = 1;
					start_attr_value = 0;

					memset(attr_tag_value, '\0', max_attr_tag_value * sizeof(char));
					memset(attr_tag_name, '\0', max_attr_tag_name * sizeof(char));
				} else
					start_attr_value = 1;

				search_token++;
				continue;
			}

			if (!start_attr_value) {
				search_token++;
				continue;
			}

			attr_tag_value[attr_tag_value_index++] = (*curr_line)[search_token];

			attr_tag_value = resize_array(attr_tag_value, &max_attr_tag_value, attr_tag_value_index, sizeof(char));
			attr_tag_value[attr_tag_value_index] = '\0';
		}

		search_token++;
	}

	free(attr_tag_name);
	free(attr_tag_value);

	add_token_children(parent_tree, new_tree);

	tag_read.new_search_token = search_token + 1;
	tag_read.type = (*curr_line)[search_token - 1] == '/';
	tag_read.update_str_read = str_read;
	return tag_read;
}

int tokenizeMETA(FILE *file, char *str_read, yomu_t *curr_tree) {
	int search_token = 0;

	size_t *buffer_size = malloc(sizeof(size_t));
	*buffer_size = sizeof(char) * 123;
	char **buffer_reader = malloc(sizeof(char *));
	*buffer_reader = malloc(*buffer_size);

	int read_len = 0;
	int line = 0;

	while((read_len = read_newline(buffer_reader, buffer_size, file, str_read)) != -1) {
		// move str_read forward
		str_read += read_len * sizeof(char);

		search_token = 0;
		char *curr_line = *buffer_reader;

		while (curr_line[search_token] != '\n' && curr_line[search_token] != '\0') {
			if (curr_line[search_token] == '<') {
				if (curr_line[search_token + 1] == '/') { // close tag
					// return to parent tree instead

					// check tag name matches curr_tree tag
					curr_tree = grab_token_parent(curr_tree);

					search_token = find_close_tag(file, str_read, buffer_reader, buffer_size, search_token) + 1;

					continue;
				}

				tag_reader tag_read = read_tag(curr_tree, file, str_read, buffer_reader, buffer_size, search_token);
				search_token = tag_read.new_search_token;

				str_read = tag_read.update_str_read;

				// depending on tag_read.type, choose specific path:
				if (tag_read.type == 0) {
					// add pointer to sub tree within data:
					add_token_rolling_data(curr_tree, '<');
					add_token_rolling_data(curr_tree, '>');
					
					curr_tree = grab_token_children(curr_tree);
				}

				continue;
			}

			// otherwise add character to the curr_tree
			add_token_rolling_data(curr_tree, curr_line[search_token]);

			search_token++;
		}
	}

	free(buffer_size);
	free(buffer_reader[0]);
	free(buffer_reader);

	return 0;
}

/*
	reader_type chooses what kind of reading is to be done:
	'f' for file reading
	's' for char * reading
*/
yomu_t *tokenize(char *filename, char reader_type) {
	FILE *file = NULL;

	if (reader_type == 'f')
		file = fopen(filename, "r");

	char *root_tag = malloc(sizeof(char) * 5);
	strcpy(root_tag, "root");
	yomu_t *curr_tree = create_token(root_tag, NULL);

	tokenizeMETA(file, filename, curr_tree);

	if (file)
		fclose(file);

	return curr_tree;
}

int id_match(yomu_t *y, char *id) {
	return has_attr_value(y->attribute, y->attr_tag_index, "id", id);
}

int class_match(yomu_t *y, char *class) {
	int class_index;
	for (class_index = 0; class_index < y->attr_tag_index; class_index += 2) {
		if (strcmp(y->attribute[class_index], "class") == 0)
			break;
	}

	if (class_index == y->attr_tag_index) // no class attribute
		return 0;

	// separate all class options
	int *class_list_len = malloc(sizeof(int)), *yomu_class_len = malloc(sizeof(int));
	char **class_list = split_string(class, '.', class_list_len, "-c");
	char **yomu_class = split_string(y->attribute[class_index + 1], ' ', yomu_class_len, "-c");

	// make sure each class in class_list occurs in yomu_class
	for (int check_class_list_match = 0; check_class_list_match < *class_list_len; check_class_list_match++) {

		int find_yomu_class;
		for (find_yomu_class = 0; find_yomu_class < *yomu_class_len; find_yomu_class++) {
			if (strcmp(class_list[check_class_list_match], yomu_class[find_yomu_class]) == 0)
				break;
		}

		// if find_yomu_class == *yomu_class_len, that means there was not an occurrence of
		// the class_list class, so this should return false
		if (find_yomu_class == *yomu_class_len)
			return 0;	
	}

	// otherwise
	return 1;
}

int tag_match(yomu_t *y, char *tag) {
	return strcmp(y->tag, tag) == 0;
}

typedef struct Match {
	char *match_tag; // used as input to the following function
	int (*match)(yomu_t *, char *);
} match_t;

match_t match_create(int (*match)(yomu_t *, char *), char *match_tag) {
	match_t new_match = {
		.match = match,
		.match_tag = match_tag
	};

	return new_match;
}

/* this will search through the char *match_param to compute all of the that are required
	for example:

		".classname h1"

	will return an array of match_t's:

	[
		class_match,
		tag_match,
		etc.
	]

	so then when running compute_matches, first get the results with the first found match,
	then using the first results, find the sub results of each result and so on and so forth
*/
match_t *create_matches(char *match_param, int *match_param_length) {
	int *sub_match_param_length = malloc(sizeof(int));
	char **sub_match_params = split_string(match_param, ' ', sub_match_param_length, "-c");

	*match_param_length = *sub_match_param_length;
	match_t *return_matches = malloc(sizeof(match_t) * *match_param_length);

	// search through sub_match_params to see which function is needed
	for (int find_match = 0; find_match < *match_param_length; find_match++) {
		// check first value in the char * to decide which type we are searching for:
		if (sub_match_params[find_match][0] == '.') // class
			return_matches[find_match] = match_create(class_match, sub_match_params[find_match]);
		else if (sub_match_params[find_match][0] == '#') // id
			return_matches[find_match] = match_create(id_match, sub_match_params[find_match]);
		else // tag
			return_matches[find_match] = match_create(tag_match, sub_match_params[find_match]);
	}

	return return_matches;
}

yomu_t **compute_matches(yomu_t *y, char *match_param, int *length, int depth) {
	int *match_param_length = malloc(sizeof(int));
	// compute matches
	match_t *matches = create_matches(match_param, match_param_length);

	if (*match_param_length == 0) // no matches?
		return NULL;

	// for each match:
	int *yomu_len = malloc(sizeof(int)), *yomu_prev_len = malloc(sizeof(int)), *yomu_buffer_len = malloc(sizeof(int));
	yomu_t **yomu_match, **yomu_prev = NULL, **yomu_buffer;
	yomu_match = grab_tokens_by_match(y, matches[0].match, matches[0].match_tag, yomu_len, depth);
	for (int find_match = 1; find_match < *match_param_length; find_match++) {
		if (yomu_prev)
			free(yomu_prev);
		*yomu_prev_len = *yomu_len;
		yomu_prev = yomu_match;

		// setup up a reader for each token in yomu_prev
		*yomu_len = 0;
		for (int read_prev = 0; read_prev < *yomu_prev_len; read_prev++) {
			yomu_buffer = grab_tokens_by_match(yomu_prev[read_prev], matches[find_match].match, matches[find_match].match_tag, yomu_buffer_len, depth);
			free(matches[find_match].match_tag);

			for (int copy_buffer_to_match = 0; copy_buffer_to_match < *yomu_buffer_len; copy_buffer_to_match++) {
				yomu_match[*yomu_len] = yomu_buffer[copy_buffer_to_match];
				*yomu_len++;
			}
		}
	}

	// copy yomu_len in length
	*length = *yomu_len;

	// free all allocations
	free(match_param_length);
	free(matches);

	free(yomu_len);
	free(yomu_prev_len);
	free(yomu_buffer_len);

	return yomu_match;
}

yomu_t **compute_match_shallow(yomu_t *y, char *match_param, int *length) {
	return compute_matches(y, match_param, length, 0);
}

yomu_t **compute_match_deep(yomu_t *y, char *match_param, int *length) {
	return compute_matches(y, match_param, length, 1);
}

yomu_t *compute_match(yomu_t *y, char *match_param, int direction) {
	int *match_param_length = malloc(sizeof(int));
	// compute matches
	match_t *matches = create_matches(match_param, match_param_length);

	if (*match_param_length == 0) // no matches?
		return NULL;

	yomu_t *yomu_match = grab_token_by_match(y, matches[0].match, matches[0].match_tag, direction);

	// free matches (in case there are many):
	for (int free_match = 0; free_match < *match_param_length; free_match++) {
		free(matches[free_match].match_tag);
	}

	free(match_param_length);
	free(matches);

	return yomu_match;
}

/*
	searches for first occurence of a match starting with the children of y
	direction: defines if the search starts at the end or the beginning:
	0 for beginning
	1 for end
*/
yomu_t *grab_first_token_by_match(yomu_t *y, char *match_param) {
	return compute_match(y, match_param, 0);
}

yomu_t *grab_last_token_by_match(yomu_t *y, char *match_param) {
	return compute_match(y, match_param, 1);
}

yomu_f yomu = (yomu_f){
	.parse = tokenize,

	.children = compute_match_shallow,
	.find = compute_match_deep,
	.first = grab_first_token_by_match,
	.last = grab_last_token_by_match,

	.parent = grab_token_parent,

	.attr = {
		.set = set_attr,
		.get = token_attr
	},

	.hasClass = token_has_classname,

	.update = update_token,
	.read = read_token,

	.destroy = destroy_token
};

/* HELPER FUNCTIONS */
int is_range(char c) {
	return 1;
}

char **split_string(char *full_string, char delimeter, int *arr_len, char *extra, ...) {
	va_list param;
	va_start(param, extra);

	int **minor_length = NULL;
	int (*is_delim)(char, char *) = NULL;
	char *multi_delims = NULL;

	int (*is_range)(char) = is_range;

	int should_lowercase = 1;

	for (int check_extra = 0; extra[check_extra]; check_extra++) {
		if (extra[check_extra] != '-')
			continue;

		if (extra[check_extra + 1] == 'l')
			minor_length = va_arg(param, int **);
		else if (extra[check_extra + 1] == 'd') {
			is_delim = va_arg(param, int (*)(char, char *));
			multi_delims = va_arg(param, char *);
		} else if (extra[check_extra + 1] == 'r') {
			is_range = va_arg(param, int (*)(char));
		} else if (extra[check_extra + 1] == 'c')
			should_lowercase = 0;
	}

	int arr_index = 0;
	*arr_len = 8;
	char **arr = malloc(sizeof(char *) * *arr_len);

	if (minor_length)
		*minor_length = realloc(*minor_length, sizeof(int) * *arr_len);

	int *max_curr_sub_word = malloc(sizeof(int)), curr_sub_word_index = 0;
	*max_curr_sub_word = 8;
	arr[arr_index] = malloc(sizeof(char) * *max_curr_sub_word);
	arr[arr_index][0] = '\0';

	for (int read_string = 0; full_string[read_string]; read_string++) {
		if ((is_delim && is_delim(full_string[read_string], multi_delims)) || full_string[read_string] == delimeter) { // next phrase
			// check that current word has some length
			if (arr[arr_index][0] == '\0')
				continue;

			// quickly copy curr_sub_word_index into minor_length if minor_length is defined:
			if (minor_length) {
				(*minor_length)[arr_index] = curr_sub_word_index;
			}

			arr_index++;

			while (arr_index >= *arr_len) {
				*arr_len *= 2;
				arr = realloc(arr, sizeof(char *) * *arr_len);

				if (minor_length)
					*minor_length = realloc(*minor_length, sizeof(int *) * *arr_len);
			}

			curr_sub_word_index = 0;
			*max_curr_sub_word = 8;
			arr[arr_index] = malloc(sizeof(char) * *max_curr_sub_word);
			arr[arr_index][0] = '\0';

			continue;
		}

		// if not in range, skip:
		if (is_range && !is_range(full_string[read_string]))
			continue;

		// if a capital letter, lowercase
		if (should_lowercase && (int) full_string[read_string] <= 90 && (int) full_string[read_string] >= 65)
			full_string[read_string] = (char) ((int) full_string[read_string] + 32);

		arr[arr_index][curr_sub_word_index] = full_string[read_string];
		curr_sub_word_index++;

		arr[arr_index] = resize_array(arr[arr_index], max_curr_sub_word, curr_sub_word_index, sizeof(char));

		arr[arr_index][curr_sub_word_index] = '\0';
	}

	if (arr[arr_index][0] == '\0') { // free position
		free(arr[arr_index]);

		arr_index--;
	} else if (minor_length)
			(*minor_length)[arr_index] = curr_sub_word_index;

	*arr_len = arr_index + 1;

	free(max_curr_sub_word);

	return arr;
}