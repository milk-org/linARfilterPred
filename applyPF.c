/**
 * @file    applyPF.c
 * @brief   Apply predictive filter
 *
 *
 */


#include "CommandLineInterface/CLIcore.h"



#ifdef HAVE_CUDA
#include "cudacomp/cudacomp.h"
#endif



static uint64_t *AOloopindex;

static char *indata;
static char *inmask;

static char *PFmat;

static char *outdata;
static char *outmask;


static char *GPUsetstr;
static long  fpi_GPUsetstr;




static CLICMDARGDEF farg[] = {
    {// AO loop index - used for automatic naming of streams aolX_
     CLIARG_UINT64,
     ".AOloopindex",
     "AO loop index",
     "0",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &AOloopindex,
     NULL},
    {// Input stream
     CLIARG_STREAM,
     ".indata",
     "input data stream",
     "inim",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &indata,
     NULL},
    {// Input stream active mask
     CLIARG_STREAM,
     ".inmask",
     "input data mask",
     "inmask",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &inmask,
     NULL},
    {// Prediction filter matrix
     CLIARG_STREAM,
     ".PFmat",
     "predictive filter matrix",
     "PFmat",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &PFmat,
     NULL},
    {// Output stream
     CLIARG_STREAM,
     ".outdata",
     "output data stream",
     "outPF",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &outdata,
     NULL},
    {// Output mask
     CLIARG_STREAM,
     ".outmask",
     "output data mask",
     "outmask",
     CLIARG_VISIBLE_DEFAULT,
     (void **) &outmask,
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


#ifdef HAVE_CUDA
    int status;
    int GPUstatus[100];
    int GPUMATMULTCONFindex = 2;
#endif


    // Connect to 2D input stream
    //
    IMGID imgin = mkIMGID_from_name(indata);
    resolveIMGID(&imgin, ERRMODE_ABORT);
    long NBmodeINmax = imgin.md->size[0] * imgin.md->size[1];

    // connect to 2D predictive filter (PF) matrix
    //
    IMGID imgPFmat = mkIMGID_from_name(PFmat);
    resolveIMGID(&imgPFmat, ERRMODE_ABORT);
    long NBmodeOUT = imgPFmat.md->size[1];

    list_image_ID();



    // Input mask
    // 0: inactive input
    // 1: active input
    //
    IMGID imginmask = mkIMGID_from_name(inmask);
    resolveIMGID(&imginmask, ERRMODE_WARN);

    long  NBinmaskpix = 0;
    long *inmaskindex;
    if (imginmask.ID != -1)
    {
        NBinmaskpix = 0;
        for (uint32_t ii = 0;
             ii < imginmask.md->size[0] * imginmask.md->size[1];
             ii++)
            if (imginmask.im->array.SI8[ii] == 1)
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
        for (uint32_t ii = 0;
             ii < imginmask.md->size[0] * imginmask.md->size[1];
             ii++)
            if (imginmask.im->array.SI8[ii] == 1)
            {
                inmaskindex[NBinmaskpix] = ii;
                NBinmaskpix++;
            }
        //printf("Number of active input modes  = %ld\n", NBinmaskpix);
    }
    else
    {
        NBinmaskpix = NBmodeINmax;
        printf("no input mask -> assuming NBinmaskpix = %ld\n", NBinmaskpix);

        inmaskindex = (long *) malloc(sizeof(long) * NBinmaskpix);

        for (uint32_t ii = 0;
             ii < imginmask.md->size[0] * imginmask.md->size[1];
             ii++)
        {
            inmaskindex[NBinmaskpix] = ii;
        }
    }
    long NBmodeIN = NBinmaskpix;




    long NBPFstep = imgPFmat.md->size[0] / NBmodeIN;

    printf("Number of active input modes  = %ld  / %ld\n",
           NBmodeIN,
           NBmodeINmax);
    printf("Number of output modes        = %ld\n", NBmodeOUT);
    printf("Number of time steps          = %ld\n", NBPFstep);


    // create input buffer holding recent input values
    //
    printf("Creating input buffer\n");
    IMGID imginbuff;
    printf("Creating input buffer\n");
    copyIMGID(&imgin, &imginbuff);
    printf("naxis = %d\n", imginbuff.naxis);
    imginbuff.naxis++;
    printf("naxis = %d\n", imginbuff.naxis);
    printf("Creating input buffer\n");
    imginbuff.md->size[imginbuff.naxis - 1] = NBPFstep;
    printf("naxis = %d\n", imginbuff.naxis);
    for (int ax = 0; ax < imginbuff.naxis; ax++)
    {
        printf("   %d  %d\n", ax, imginbuff.md->size[ax]);
        fflush(stdout);
    }
    strcpy(imginbuff.name, "inbuff");
    createimagefromIMGID(&imginbuff);


    list_image_ID();

    // OUTPUT

    // Connect to output mask and data stream
    //
    IMGID imgout = mkIMGID_from_name(outdata);
    resolveIMGID(&imgout, ERRMODE_WARN);

    IMGID imgoutmask = mkIMGID_from_name(outmask);
    resolveIMGID(&imgoutmask, ERRMODE_WARN);


    list_image_ID();

    // If both outdata and outmask exist, check they are consistent
    if ((imgout.ID != -1) && (imgoutmask.ID != -1))
    {
        if (IMGIDcompare(imgout, imgoutmask) != 0)
        {
            PRINT_ERROR("images %s and %s are incompatible\n",
                        outdata,
                        outmask);
            DEBUG_TRACE_FEXIT();
        }
    }
    else
    {
        if (imgout.ID != -1)
        {
            // outdata exists, but outmask does not
            //
            // Check that outdata is big enough
            //
            if (imgout.md->nelement < (uint64_t) NBmodeOUT)
            {
                PRINT_ERROR("images %s too small to contain %ld output modes\n",
                            outdata,
                            NBmodeOUT);
                DEBUG_TRACE_FEXIT();
            }
            imcreatelikewiseIMGID(&imgoutmask, &imgout);
            for (uint32_t ii = 0; ii < NBmodeOUT; ii++)
            {
                imgoutmask.im->array.SI8[ii] = 1;
            }
        }
        else if (imgoutmask.ID != -1)
        {
            // outmask exists, but outdata does not
            // create outdata according to outmask
            //
            copyIMGID(&imgoutmask, &imgout);
            imgout.datatype = _DATATYPE_FLOAT;
            createimagefromIMGID(&imgout);
        }
        else
        {
            // Neither outdata nor outmask exist
            // 2D array
            //
            imgout = stream_connect_create_2Df32(outdata, NBmodeOUT, 1);
            imgout = stream_connect_create_2Df32(outmask, NBmodeOUT, 1);
            for (uint32_t ii = 0; ii < NBmodeOUT; ii++)
            {
                imgoutmask.im->array.SI8[ii] = 1;
            }
        }
    }

    list_image_ID();




    // Identiy GPUs
    //
    int  NBGPUmax = 20;
    int  NBGPU    = 0;
    int *GPUset   = (int *) malloc(sizeof(int) * NBGPUmax);
    for (int gpui = 0; gpui < NBGPUmax; gpui++)
    {
        char gpuistr[5];
        sprintf(gpuistr, ":%d:", gpui);
        if (strstr(GPUsetstr, gpuistr) != NULL)
        {
            GPUset[NBGPU] = gpui;
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


    if (NBGPU > 0) // if using GPU
    {

#ifdef HAVE_CUDA
        if (processinfo->loopcnt == 0)
        {
            printf("INITIALIZE GPU(s)\n\n");
            fflush(stdout);

            GPU_loop_MultMat_setup(GPUMATMULTCONFindex,
                                   imgPFmat.name,
                                   imginbuff.name,
                                   imgout.name,
                                   NBGPU,
                                   GPUset,
                                   0,
                                   1,
                                   1,
                                   *AOloopindex);

            printf("INITIALIZATION DONE\n\n");
            fflush(stdout);
        }
        GPU_loop_MultMat_execute(GPUMATMULTCONFindex,
                                 &status,
                                 &GPUstatus[100],
                                 1.0,
                                 0.0,
                                 0,
                                 0);
#endif
    }
    else // if using CPU
    {
        // compute output : matrix vector mult with a CPU-based loop
        imgout.md->write = 1;
        for (long mi = 0; mi < NBmodeOUT; mi++)
        {
            imgout.im->array.F[mi] = 0.0;
            for (uint32_t ii = 0; ii < NBmodeIN * NBPFstep; ii++)
            {
                imgout.im->array.F[mi] +=
                    imginbuff.im->array.F[ii] *
                    imgPFmat.im->array.F[mi * NBmodeIN * NBPFstep + ii];
            }
        }
        COREMOD_MEMORY_image_set_sempost_byID(imgout.ID, -1);
        imgout.md->write = 0;
        imgout.md->cnt0++;
    }


    // Update time buffer input
    // do this now to save time when semaphore is posted
    for (long tstep = NBPFstep - 1; tstep > 0; tstep--)
    {
        // tstep-1 -> tstep
        for (long mi = 0; mi < NBmodeIN; mi++)
        {
            imginbuff.im->array.F[NBmodeIN * tstep + mi] =
                imginbuff.im->array.F[NBmodeIN * (tstep - 1) + mi];
        }
    }




    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(GPUset);
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
