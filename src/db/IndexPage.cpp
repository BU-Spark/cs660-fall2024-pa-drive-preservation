#include <db/IndexPage.hpp>
#include <stdexcept>
#include <algorithm>

using namespace db;

// Constructor to initialize an IndexPage
// Sets up the header and calculates the available capacity based on page size
// Initializes pointers to manage keys and child pages in this index page
IndexPage::IndexPage(Page &page) {
    
    header = reinterpret_cast<IndexPageHeader*>(page.data());
    header->size = 0;  
    capacity = (DEFAULT_PAGE_SIZE - sizeof(IndexPageHeader)) / (sizeof(int) + sizeof(size_t)) - 1;  

    // Set up pointers to the keys and children sections within the page
    keys = reinterpret_cast<int*>(page.data() + sizeof(IndexPageHeader));
    children = reinterpret_cast<size_t*>(page.data() + sizeof(IndexPageHeader) + capacity * sizeof(int));
}

// Function to insert a key and its child page into the index page
// Finds the correct position to keep keys in order, shifts entries to make space if needed
// Increments size and returns true if the page is full after insertion
bool IndexPage::insert(int key, size_t child) {
    int pos = std::lower_bound(keys, keys + header->size, key) - keys;

    if (header->size == capacity) {
        return true;  // Page is full, a split is needed
    }

    // Shift keys and children to make space for the new entry
    for (int i = header->size; i > pos; --i) {
        keys[i] = keys[i - 1];
        children[i + 1] = children[i];
    }

    // Insert the new key and child
    keys[pos] = key;
    children[pos + 1] = child;
    header->size++;  
    return header->size == capacity;  
}

// Function to split an index page into two when it is full
// Divides the page into two parts at the middle key, moving one half to a new page
// The middle key is copied to be inserted in the parent page, and the function returns it
int IndexPage::split(IndexPage &new_page) {
    int mid = header->size / 2;  // Determine midpoint
    int mid_key = keys[mid];  // Key to promote to parent

    // Set sizes for each page after split
    new_page.header->size = header->size - mid - 1;
    header->size = mid;

    // Move the second half of keys and children to the new page
    for (int i = 0; i < new_page.header->size; ++i) {
        new_page.keys[i] = keys[mid + 1 + i];
        new_page.children[i] = children[mid + 1 + i];
    }
    new_page.children[new_page.header->size] = children[header->size + 1];

    return mid_key;  // Return the middle key
}
