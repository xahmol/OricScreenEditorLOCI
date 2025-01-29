#ifndef __DIR_H_
#define __DIR_H_

// Placed in:
// Overlay 3

// Pane location and height
#define PANE_HEIGHT 15

// Structs and variables

// Structure for directory meta data
struct DirMeta
{
    unsigned next;        // Pointer to next element
    unsigned prev;        // Pointer to previous element
    unsigned char type;   // Type: 1=dir, 2=DSK, 3=TAP, 4=ROM, 5=LCE
    unsigned char select; // Select: 0=not selected, 1=selected
    unsigned char length; // Length of name
};

// Structure for directory element
struct DirElement
{
    char name[64];       // Entry name
    struct DirMeta meta; // Meta data
};
extern struct DirElement presentdirelement;

// Structure for active directory for both panes
struct Directory
{
    unsigned firstelement;  // Pointer to first element
    unsigned firstprint;    // Pointer to first element to print
    unsigned lastprint;     // Pointer to last element to print
    unsigned present;       // Pointer to active element
    unsigned char drive;    // Drive number
    unsigned char position; // Position in directory
    char path[256];         // Path
    unsigned address;       // Address in XRAM for next entry
};
extern struct Directory presentdir;

// Directory reading variables
extern DIR *dir;
extern struct dirent *file;
extern char dir_entry_types[8][8]; // Directory entry type name text strings

// Application variables
extern unsigned present;     // Present element
extern unsigned previous;    // Previous element
extern unsigned next;        // Next element
extern unsigned char sort;   // Sort flag
extern unsigned char filter; // Filter

// Function prototypes
void dir_get_element(unsigned address);
void dir_save_element(unsigned address);
void dir_read(unsigned char filter);
void dir_print_id_and_path();
void dir_print_entry(unsigned char printpos);
void dir_draw(unsigned char readdir);
void dir_get_next_drive();
void dir_get_prev_drive();
void dir_go_down();
void dir_go_up();
void dir_pagedown();
void dir_pageup();
void dir_top();
void dir_bottom();
void dir_last_of_page();
void dir_gotoroot();
void dir_parentdir();
void dir_togglesort();
unsigned char dir_browse(unsigned char filter);

#endif // __DIR_H_