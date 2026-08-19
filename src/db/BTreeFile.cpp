#include <db/BTreeFile.hpp>
#include <db/Database.hpp>
#include <db/IndexPage.hpp>
#include <db/LeafPage.hpp>
#include <stack>
#include <stdexcept>
#include <variant>

using namespace db;

constexpr size_t INVALID_PAGE_ID = SIZE_MAX; // Define an invalid page ID for end-of-file indication

// Constructor for BTreeFile
// Sets up the B-tree file with a specific name, tuple descriptor, and key index to help order the data
BTreeFile::BTreeFile(const std::string &name, const TupleDesc &td, size_t key_index)
    : DbFile(name, td), key_index(key_index) {}

// Function to insert a tuple into the B-tree
// Navigates to the correct leaf page based on the key and inserts the tuple there
// If the leaf is full, it splits and moves up the tree, creating a new root if needed
void BTreeFile::insertTuple(const Tuple &t) {
  BufferPool &bufferPool = getDatabase().getBufferPool();
  Page &root_page = bufferPool.getPage(PageId{name, root_id});
  IndexPage root_index(root_page);

  size_t current_page_id = root_id;
  Page *current_page = &root_page;

  std::stack<size_t> parent_stack;

  while (root_index.header->index_children) {
    parent_stack.push(current_page_id); // Track the current page as a parent
    size_t child_index = 0;
    for (; child_index < root_index.header->size; ++child_index) {

      if (std::holds_alternative<int>(t.get_field(key_index))) {
        int tuple_key = std::get<int>(t.get_field(key_index));
        int page_key = root_index.keys[child_index];
        if (tuple_key < page_key) {
          break;
        }
      } else {
        throw std::runtime_error("Key comparison error: expected integer fields in tuple.");
      }
    }
    current_page_id = root_index.children[child_index];
    current_page = &bufferPool.getPage(PageId{name, current_page_id});
    root_index = IndexPage(*current_page);
  }

  LeafPage leaf_page(*current_page, td, key_index);
  if (leaf_page.insertTuple(t)) {
    // Handle leaf page split
    Page &new_leaf_page = bufferPool.getPage(PageId{name, numPages++});
    LeafPage new_leaf(new_leaf_page, td, key_index);
    int split_key = leaf_page.split(new_leaf);

    bufferPool.markDirty(PageId{name, current_page_id});
    bufferPool.markDirty(PageId{name, numPages - 1});

    // Propagate the split up the tree
    while (true) {
      if (root_index.insert(split_key, numPages - 1))
        break;

      Page &new_index_page = bufferPool.getPage(PageId{name, numPages++});
      IndexPage new_index(new_index_page);
      split_key = root_index.split(new_index);

      bufferPool.markDirty(PageId{name, current_page_id});
      bufferPool.markDirty(PageId{name, numPages - 1});

      if (parent_stack.empty()) {
        // If stack is empty, we reached the root, create a new root
        Page &new_root_page = bufferPool.getPage(PageId{name, numPages++});
        IndexPage new_root(new_root_page);
        new_root.insert(split_key, numPages - 2);
        new_root.children[0] = root_id;
        bufferPool.markDirty(PageId{name, numPages - 1});
        break;
      }
      current_page_id = parent_stack.top();
      parent_stack.pop();
      current_page = &bufferPool.getPage(PageId{name, current_page_id});
      root_index = IndexPage(*current_page);
    }
  } else {
    bufferPool.markDirty(PageId{name, current_page_id});
  }
}

// Function to retrieve a tuple at the position specified by an iterator
// Verifies that the slot is within the valid range, then gets and returns the tuple from the leaf page
Tuple BTreeFile::getTuple(const Iterator &it) const {
  // The function uses the iterator (it) to locate the correct page by accessing it through the BufferPool.
  BufferPool &bufferPool = getDatabase().getBufferPool();
  // The PageId is constructed from the BTree file's name and the page ID held by the iterator (it.page).
  Page &page = bufferPool.getPage(PageId{name, it.page});
  LeafPage leaf_page(page, td, key_index);

  if (it.slot >= leaf_page.header->size) {
    throw std::runtime_error("Invalid slot index");
  }

  return leaf_page.getTuple(it.slot);
}

// Function to advance the iterator to the next tuple
// Moves to the next tuple in the page, or goes to the next leaf page if needed
void BTreeFile::next(Iterator &it) const {
  BufferPool &bufferPool = getDatabase().getBufferPool();
  Page &page = bufferPool.getPage(PageId{name, it.page});
  LeafPage leaf_page(page, td, key_index);

  if (++it.slot >= leaf_page.header->size) {
    if (leaf_page.header->next_leaf == INVALID_PAGE_ID) {
      // Set iterator to end state by updating page and slot directly
      it.page = INVALID_PAGE_ID;
      it.slot = 0;
    } else {
      it.page = leaf_page.header->next_leaf;
      it.slot = 0;
    }
  }
}

// Function to get the iterator to the first tuple in the B-tree
// Goes down the left side of the tree to reach the leftmost leaf page
// If there are no tuples, returns an end iterator
Iterator BTreeFile::begin() const {
  BufferPool &bufferPool = getDatabase().getBufferPool();
  Page *page = &bufferPool.getPage(PageId{name, root_id});
  IndexPage root_index(*page);

  while (root_index.header->index_children) {
    page = &bufferPool.getPage(PageId{name, root_index.children[0]});
    root_index = IndexPage(*page);
  }

  LeafPage leaf_page(*page, td, key_index);
  if (leaf_page.header->size == 0) {
    return end(); // Return end iterator if the BTree is empty
  }

  return Iterator(*this, root_id, 0); // Initialize iterator with the first tuple in the leftmost leaf
}

// Function to get an iterator representing the end of the B-tree
// Returns an iterator that points to an invalid page, marking the end of traversal
Iterator BTreeFile::end() const {
  return Iterator(*this, INVALID_PAGE_ID, 0); // Initialize iterator to indicate end of file
}

// Empty function for deleting a tuple from the B-tree
void BTreeFile::deleteTuple(const Iterator &it) {}
