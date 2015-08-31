
#include <string.h> //strcmp() for "ILDA" header
#include <lzr.h>
#include "lzr_ilda.h"



/*
 * Point conversion
 * These were done with macros to handle both 2D and 3D types
 *
 * These assume that all point types have `x` and `y` members
 * extra dimensions are outright ignored (3D is orthographically projected)
 */

//arguments: (ilda_parser*, ilda_point_2d_true | ilda_point_3d_true, lzr_point)
#define ilda_true_to_lzr(ilda, ilda_p, lzr_p) {     \
    (lzr_p).x = (double) (ilda_p).x / INT16_MAX;    \
    (lzr_p).y = (double) (ilda_p).y / INT16_MAX;    \
    (lzr_p).r = (ilda_p).r;                         \
    (lzr_p).g = (ilda_p).g;                         \
    (lzr_p).b = (ilda_p).b;                         \
    (lzr_p).i = UINT8_MAX;                          \
    if((ilda_p).status.blanked)                     \
        LZR_POINT_BLANK((lzr_p));                   \
}

//Arguments: (ilda_parser*, ilda_point_2d_indexed | ilda_point_3d_indexed, lzr_point)
#define ilda_indexed_to_lzr(ilda, ilda_p, lzr_p) {                 \
    (lzr_p).x = (double) (ilda_p).x / INT16_MAX;                   \
    (lzr_p).y = (double) (ilda_p).y / INT16_MAX;                   \
    ilda_color c = current_palette_get((ilda), (ilda_p).color);    \
    (lzr_p).r = c.r;                                               \
    (lzr_p).g = c.g;                                               \
    (lzr_p).b = c.b;                                               \
    (lzr_p).i = UINT8_MAX;                                         \
    if((ilda_p).status.blanked)                                    \
        LZR_POINT_BLANK((lzr_p));                                  \
}



//reads a single record, and error checks the size
static bool read_record(ilda_parser* ilda, void* buffer, size_t buffer_size)
{
    size_t r = fread(buffer, 1, buffer_size, ilda->f);

    //make sure we got everything...
    if(r != buffer_size)
    {
        perror("Encountered incomplete record");
        return false;
    }

    return true;
}


/*
 * Section body readers
 * These functions iterate over their section's records
 *
 * Points are saved off to the given buffer.
 * Colors are placed in that projector's color palette array
 */

// -------------------- Format 0 --------------------
static bool read_3d_indexed(ilda_parser* ilda, lzr_frame* buffer)
{
    size_t n_points = NUMBER_OF_RECORDS(ilda);
    if(n_points > LZR_FRAME_MAX_POINTS)
    {
        perror("Too many points for lzr_frame. Partial frame loaded.");
        n_points = LZR_FRAME_MAX_POINTS;
    }

    //save the frame length to the user's buffer
    buffer[ilda->current_frame].n_points = n_points;

    //iterate over the records
    for(size_t i = 0; i < n_points; i++)
    {
        lzr_point lzr_p;
        ilda_point_3d_indexed p;

        if(!read_record(ilda, (void*) &p, sizeof(ilda_point_3d_indexed)))
            return false;

        //convert the ILDA point to a lzr_point
        endian_3d(&p);
        ilda_indexed_to_lzr(ilda, p, lzr_p);

        //save the new point to the user's buffer
        buffer[ilda->current_frame].points[i] = lzr_p;

        //if this was the last point, stop
        if(p.status.last_point)
            break;
    }

    return true;
}

// -------------------- Format 2 --------------------
static bool read_colors(ilda_parser* ilda)
{
    current_palette_init(ilda, NUMBER_OF_RECORDS(ilda));

    for(size_t i = 0; i < NUMBER_OF_RECORDS(ilda); i++)
    {
        ilda_color c;

        //read one color record
        if(!read_record(ilda, (void*) &c, sizeof(ilda_color)))
            return false;

        //store the new color
        current_palette_set(ilda, i, c);
    }

    return true;
}


/*
 * Header Reader
 */
static bool read_header(ilda_parser* ilda)
{
    //read one ILDA header
    size_t r = fread((void*) &(ilda->h),
                     1,
                     sizeof(ilda_header),
                     ilda->f);

    //did we capture a full header?
    if(r != sizeof(ilda_header))
    {
        if(r > 0)
            perror("Encountered incomplete ILDA section header");

        return false;
    }

    //is it prefixed with "ILDA"?
    if(strcmp("ILDA", ilda->h.ilda) != 0)
    {
        perror("Section header did not contain \"ILDA\"");
        return false;
    }

    //correct for the big-endianness of the file
    endian_header(&(ilda->h));

    return true;
}



//reads a single section (frame) of the ILDA file
//returns boolean for whether to continue parsing
static bool read_section_for_projector(ilda_parser* ilda, uint8_t pd, lzr_frame* buffer)
{
    //pull out the next header
    if(!read_header(ilda))
        return false;

    //this section contains no records, halt parsing
    if(NUMBER_OF_RECORDS(ilda) == 0)
        return false;

    //if this section isn't marked for projector we're looking for, skip it 
    if(PROJECTOR(ilda) != pd)
        return true;

    //invoke the corresponding parser for this section type
    switch(FORMAT(ilda))
    {
        case 0:
            if(!read_3d_indexed(ilda, buffer)) return false;
            break;
        case 1:
            break;
        case 2:
            if(!read_colors(ilda)) return false;
            break;
        case 4:
            break;
        case 5:
            break;
        default:
            /*
                In this case, we can't skip past an unknown
                section type, since we don't know the record size.
            */
            return false;
    }

    return true;
}

//skips from the end of the current header, to the start
//of the next header
static bool skip_to_next_section(ilda_parser* ilda)
{
    int skip_bytes = 0;

    switch(FORMAT(ilda))
    {
        case 0: skip_bytes = sizeof(ilda_point_3d_indexed); break;
        case 1: skip_bytes = sizeof(ilda_point_2d_indexed); break;
        case 2: skip_bytes = sizeof(ilda_color);            break;
        case 4: skip_bytes = sizeof(ilda_point_3d_true);    break;
        case 5: skip_bytes = sizeof(ilda_point_2d_true);    break;
        default:
            /*
                In this case, we can't skip past an unknown
                section type, since we don't know the record size.
            */
            return false;
    }

    skip_bytes *= NUMBER_OF_RECORDS(ilda);

    return (fseek(ilda->f, skip_bytes, SEEK_CUR) == 0);
}


//this function looks at each section header in the ILDA file,
//and caches the number of frames per projector.
static void scan_file(ilda_parser* ilda)
{
    //wipe the projector data (color and frame arrays)
    for(size_t pd = 0; pd < MAX_PROJECTORS; pd++)
    {
        ilda_projector* proj = GET_PROJECTOR_DATA(ilda, pd);
        proj->colors = NULL;
        proj->n_colors = 0;
        proj->n_frames = 0;
    }

    //read all of the headers, and take notes
    while(true)
    {
        //pull out the next header
        if(!read_header(ilda))
            break;

        //this section contains no records, halt parsing
        if(NUMBER_OF_RECORDS(ilda) == 0)
            break;

        //if the section carries point data, count it as a frame
        switch(FORMAT(ilda))
        {
            case 0:
            case 1:
            case 4:
            case 5:
                //increment the number of frames for this projector
                ilda->projectors[PROJECTOR(ilda)].n_frames++;

            //ignore all other section types
        }

        skip_to_next_section(ilda);
    }
}


/******************************************************************************/
/*  Public Functions                                                          */
/******************************************************************************/


void* lzr_ilda_read(char* filename)
{
    //init a parser
    ilda_parser* ilda = (ilda_parser*) malloc(sizeof(ilda_parser));
    ilda->f = fopen(filename, "rb");

    if(ilda->f == NULL)
    {
        perror("Failed to open file");
        return NULL;
    }

    //scan the file
    scan_file(ilda);

    return (void*) ilda;
}




void lzr_ilda_close(void* f)
{
    ilda_parser* ilda = (ilda_parser*) f;
    
    //destruct the parser
    for(size_t pd = 0; pd < MAX_PROJECTORS; pd++)
    {
        ilda_projector* proj = GET_PROJECTOR_DATA(ilda, pd);
        free_projector_palette(proj);
    }

    fclose(ilda->f);
    free(ilda);
}


size_t lzr_ilda_projector_count(void* f)
{
    ilda_parser* ilda = (ilda_parser*) f;

    size_t n = 0;
    for(size_t pd = 0; pd < MAX_PROJECTORS; pd++)
    {
        ilda_projector* proj = GET_PROJECTOR_DATA(ilda, pd);
        if(proj->n_frames > 0)
            n++;
    }

    return n;
}

size_t lzr_ilda_frame_count(void* f, size_t pd)
{
    ilda_parser* ilda = (ilda_parser*) f;

    if(pd > MAX_PROJECTORS)
        return 0;

    ilda_projector* proj = GET_PROJECTOR_DATA(ilda, pd);
    return (size_t) proj->n_frames;
}


int lzr_ilda_read_frames(void* f, size_t pd, lzr_frame* buffer)
{
    ilda_parser* ilda = (ilda_parser*) f;

    //seek to the beginning of the file
    fseek(ilda->f, 0, SEEK_SET);

    //zero out the frame counter
    ilda->current_frame = 0;

    //read all sections until the end is reached
    while(true)
    {
        if(!read_section_for_projector(ilda, (uint8_t) pd, buffer))
            break;
    }

    return LZR_SUCCESS;
}