/**
 * @file    applyPF.c
 * @brief   Apply predictive filter
 *
 *
 */


#include "CommandLineInterface/CLIcore.h"



static uint64_t *AOloopindex;

static char *indatastream;

static char *PFmat;

static char *outPF;

static char *GPUsetstr;
static long  fpi_GPUsetstr;




static CLICMDARGDEF farg[] = {{CLIARG_UINT64,
                               ".AOloopindex",
                               "AO loop index",
                               "0",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &AOloopindex,
                               NULL},
                              {CLIARG_STREAM,
                               ".indata",
                               "input data stream",
                               "inim",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &indatastream,
                               NULL},
                              {CLIARG_STREAM,
                               ".PFmat",
                               "predictive filter matrix",
                               "PFmat",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &PFmat,
                               NULL},
                              {CLIARG_STR,
                               ".outPF",
                               "output stream",
                               "outPF",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &outPF,
                               NULL},
                              {CLIARG_STR,
                               ".GPUset",
                               "column-separated list of GPUs",
                               ":2:3:5:",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &GPUsetstr,
                               &fpi_GPUsetstr}};


// Optional custom configuration setup. comptbuff
// Runs once at conf startup
//
static errno_t customCONFsetup()
{
    if (data.fpsptr != NULL)
    {
    }

    return RETURN_SUCCESS;
}

// Optional custom configuration checks.
// Runs at every configuration check loop iteration
//
static errno_t customCONFcheck()
{

    if (data.fpsptr != NULL)
    {
    }

    return RETURN_SUCCESS;
}

static CLICMDDATA CLIcmddata = {
    "applyPF", "apply predictive filter", CLICMD_FIELDS_DEFAULTS};




// detailed help
static errno_t help_function()
{


    return RETURN_SUCCESS;
}




static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();


    // connect to input stream
    //
    imageID IDmodevalIN = image_ID(indatastream);
    long    NBmodeIN0   = data.image[IDmodevalIN].md[0].size[0];

    // connect to predictive filter (PF) matrix
    //
    imageID IDPFmat   = image_ID(PFmat);
    long    NBmodeOUT = data.image[IDPFmat].md[0].size[1];


    // Optional input mask
    //
    long    NBinmaskpix = 0;
    long   *inmaskindex;
    imageID IDinmask = image_ID("inmask");
    if (IDinmask != -1)
    {
        NBinmaskpix = 0;
        for (uint32_t ii = 0; ii < data.image[IDinmask].md[0].size[0]; ii++)
            if (data.image[IDinmask].array.F[ii] > 0.5)
            {
                NBinmaskpix++;
            }

        inmaskindex = (long *) malloc(sizeof(long) * NBinmaskpix);
        if (inmaskindex == NULL)
        {
            PRINT_ERROR("malloc returns NULL pointer");
            abort();
        }

        NBinmaskpix = 0;
        for (uint32_t ii = 0; ii < data.image[IDinmask].md[0].size[0]; ii++)
            if (data.image[IDinmask].array.F[ii] > 0.5)
            {
                inmaskindex[NBinmaskpix] = ii;
                NBinmaskpix++;
            }
        //printf("Number of active input modes  = %ld\n", NBinmaskpix);
    }
    else
    {
        NBinmaskpix = NBmodeIN0;
        printf("no input mask -> assuming NBinmaskpix = %ld\n", NBinmaskpix);
        create_2Dimage_ID("inmask", NBinmaskpix, 1, &IDinmask);
        for (uint32_t ii = 0; ii < data.image[IDinmask].md[0].size[0]; ii++)
        {
            data.image[IDinmask].array.F[ii] = 1.0;
        }

        inmaskindex = (long *) malloc(sizeof(long) * NBinmaskpix);
        if (inmaskindex == NULL)
        {
            PRINT_ERROR("malloc returns NULL pointer");
            abort();
        }

        for (uint32_t ii = 0; ii < data.image[IDinmask].md[0].size[0]; ii++)
        {
            inmaskindex[NBinmaskpix] = ii;
        }
    }
    long NBmodeIN = NBinmaskpix;


    long NBPFstep = data.image[IDPFmat].md[0].size[0] / NBmodeIN;

    printf("Number of input modes         = %ld\n", NBmodeIN0);
    printf("Number of active input modes  = %ld\n", NBmodeIN);
    printf("Number of output modes        = %ld\n", NBmodeOUT);
    printf("Number of time steps          = %ld\n", NBPFstep);




    // Combined predictive filter output
    // this is where multiple blocks are assembled
    //
    char imname[STRINGMAXLEN_IMGNAME];
    WRITE_IMAGENAME(imname, "aol%ld_modevalPF", *AOloopindex);
    imageID IDoutcomb = image_ID(imname);


    // Buffer in which recent samples are stored
    //
    imageID IDINbuff;
    create_2Dimage_ID("INbuffer", NBmodeIN, NBPFstep, &IDINbuff);


    // output predicted values
    //
    imageID IDPFout = -1;
    {
        uint32_t *sizearray = (uint32_t *) malloc(sizeof(uint32_t) * 2);
        sizearray[0]        = NBmodeOUT;
        sizearray[1]        = 1;
        long naxis          = 2;
        IDPFout             = image_ID(outPF);
        if (IDPFout == -1)
        {
            create_image_ID(outPF,
                            naxis,
                            sizearray,
                            _DATATYPE_FLOAT,
                            1,
                            0,
                            0,
                            &IDPFout);
        }
        free(sizearray);
    }



    // Identiy GPUs
    int NBGPUmax = 20;
    int NBGPU    = 0;
    int gpuset[NBGPUmax];
    for (int gpui = 0; gpui < NBGPUmax; gpui++)
    {
        char gpuistr[5];
        sprintf(gpuistr, ":%d:", gpui);
        if (strstr(GPUsetstr, gpuistr) != NULL)
        {
            gpuset[NBGPU] = gpui;
            printf("Using GPU device %d\n", gpui);
            NBGPU++;
        }
    }
    if (NBGPU > 0)
    {
        printf("Using %d GPUs\n", NBGPU);
    }
    else
    {
        printf("Using CPU\n");
    }




    INSERT_STD_PROCINFO_COMPUTEFUNC_START

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(inmaskindex);

    DEBUG_TRACE_FEXIT();
    return RETURN_SUCCESS;
}




INSERT_STD_FPSCLIfunctions



    // Register function in CLI
    errno_t
    CLIADDCMD_LinARfilterPred__applyPF()
{

    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    CLIcmddata.FPS_customCONFcheck = customCONFcheck;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
