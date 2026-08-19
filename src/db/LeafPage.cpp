#include <db/LeafPage.hpp>
#include <stdexcept>
#include <cstring>  // for memcpy

using namespace db;

// Constructor to set up a LeafPage
// Sets up the page header and reserves space to hold tuples
// Calculates the page's capacity based on the available space and the size of tuples
LeafPage::LeafPage(Page &page, const TupleDesc &td, size_t key_index)
    : td(td), key_index(key_index) {
  // Allocate space for the header and data sections
  header = new LeafPageHeader;
  capacity = (DEFAULT_PAGE_SIZE - sizeof(LeafPageHeader)) / td.length();
  data = (uint8_t *)malloc(capacity * td.length());
}

// Function to insert a tuple into the leaf page
// Finds the correct spot in the page to keep tuples sorted by the key field
// Inserts the tuple, shifting existing ones if necessary, and avoids duplicates
// Returns true if the page is full after insertion, indicating a split is needed
bool LeafPage::insertTuple(const Tuple &t) {
  
  int insertPos = 0;
  bool isLargest = true;
  bool isDuplicate = false;
  int intFieldIndex = 0;

  
  for (int i = 0; i < td.size(); ++i) {
    if (t.field_type(i) == db::type_t::INT) {
      intFieldIndex = i;
      break;
    }
  }

  // Identify the correct slot for the new tuple
  for (int i = 0; i < header->size; ++i) {
    Tuple currentTuple = td.deserialize(data + i * td.length());
    if (currentTuple.get_field(intFieldIndex) > t.get_field(intFieldIndex)) {
      insertPos = i;
      isLargest = false;
      break;
    }
    if (currentTuple.get_field(intFieldIndex) == t.get_field(intFieldIndex)) {
      insertPos = i;
      isDuplicate = true;
      break;
    }
  }

  // Shift existing tuples to make space for the new tuple, if needed
  if (!isLargest && !isDuplicate) {
    for (int i = header->size; i > insertPos; --i) {
      Tuple movedTuple = td.deserialize(data + (i - 1) * td.length());
      td.serialize(data + i * td.length(), movedTuple);
    }
  } else if (isLargest && !isDuplicate) {
    insertPos = header->size;
  }

  // Insert the new tuple
  td.serialize(data + insertPos * td.length(), t);

  // Only increase the size if there was no duplicate
  if (!isDuplicate) {
    header->size++;
  }

  // Return true if the page is full and needs a split
  return header->size == capacity;
}

// Function to split a full leaf page into two pages
// Moves half of the tuples to the new page and adjusts sizes accordingly
// Returns the key of the first tuple in the new page to help with indexing
int LeafPage::split(LeafPage &new_page) {
  // Split the page by transferring half of the tuples to the new page
  header->size = capacity / 2;
  new_page.header->size = capacity - header->size;
  new_page.header->next_leaf = header->next_leaf;

  // Move tuples to the new page
  for (int i = 0; i < new_page.header->size; ++i) {
    Tuple tupleToMove = td.deserialize(data + (header->size + i) * td.length());
    new_page.td.serialize(new_page.data + i * td.length(), tupleToMove);
  }

  // Return the key of the first tuple in the new page
  return std::get<int>(td.deserialize(new_page.data).get_field(key_index));
}

// Function to get a tuple at a specific slot in the page
// Checks if the slot index is valid, then retrieves and returns the tuple
Tuple LeafPage::getTuple(size_t slot) const {
  if (slot >= header->size) {
    throw std::runtime_error("Invalid slot index");
  }
  return td.deserialize(data + slot * td.length());
}
