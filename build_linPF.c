/**
 * @file build_linPF.c
 *
 *
 */


#include <math.h>

#include "CommandLineInterface/CLIcore.h"



static char *inname;

static uint32_t *PForder;
static long      fpi_PForder;

static float *PFlatency;
static long   fpi_PFlatency;

static double *SVDeps;
static long    fpi_SVDeps;

static double *reglambda;
static long    fpi_reglambda;

static char *outPFname;

static float *loopgain;
static long   fpi_loopgain;




static CLICMDARGDEF farg[] = {{CLIARG_STR,
                               ".inname",
                               "input telemetry",
                               "indata",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &inname,
                               NULL},
                              {CLIARG_UINT32,
                               ".PForder",
                               "predictive filter order",
                               "10",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &PForder,
                               &fpi_PForder},
                              {CLIARG_FLOAT32,
                               ".PFlatency",
                               "time latency [frame]",
                               "2.7",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &PFlatency,
                               &fpi_PFlatency},
                              {CLIARG_FLOAT64,
                               ".SVDeps",
                               "SVD cutodd",
                               "0.001",
                               CLIARG_HIDDEN_DEFAULT,
                               (void **) &SVDeps,
                               &fpi_SVDeps},
                              {CLIARG_FLOAT64,
                               ".reglambda",
                               "regularization coefficient",
                               "0.001",
                               CLIARG_HIDDEN_DEFAULT,
                               (void **) &reglambda,
                               &fpi_reglambda},
                              {CLIARG_STR,
                               ".outPFname",
                               "output filter",
                               "outPF",
                               CLIARG_VISIBLE_DEFAULT,
                               (void **) &outPFname,
                               NULL},
                              {CLIARG_FLOAT32,
                               ".loopgain",
                               "loop gain",
                               "0.2",
                               CLIARG_HIDDEN_DEFAULT,
                               (void **) &loopgain,
                               &fpi_loopgain}};




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
    "mkPF", "make linear predictiv filter", CLICMD_FIELDS_DEFAULTS};




// detailed help
static errno_t help_function()
{


    return RETURN_SUCCESS;
}




static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();


    // connect to input telemetry
    //
    IMGID imgin = mkIMGID_from_name(inname);
    resolveIMGID(&imgin, ERRMODE_ABORT);




    /// ## Selecting input values

    /// The goal of this function is to build a linear link between
    /// input and output variables. \n
    /// Input variables values are provided by the input telemetry image
    /// which is first read to measure dimensions, and allocate memory.\n
    /// Note that an optional variable selection step allows only a
    /// subset of the telemetry variables to be considered.

    uint32_t nbspl    = 0;
    uint32_t xsize    = 0;
    uint32_t ysize    = 0;
    uint32_t inNBelem = 0;
    imageID  IDincp;

    switch (imgin.md->naxis)
    {

    case 2:
        /// If 2D image:
        /// - xysize <- size[0] is number of variables
        /// - nbspl <- size[1] is number of samples
        nbspl = imgin.md->size[1];
        xsize = imgin.md->size[0];
        ysize = 1;
        // copy of image to avoid input change during computation
        create_2Dimage_ID("PFin_cp",
                          imgin.md->size[0],
                          imgin.md->size[1],
                          &IDincp);
        inNBelem = imgin.md->size[0] * imgin.md->size[1];
        break;

    case 3:
        /// If 3D image
        /// - xysize <- size[0] * size[1] is number of variables
        /// - nbspl <- size[2] is number of samples
        nbspl = imgin.md->size[2];
        xsize = imgin.md->size[0];
        ysize = imgin.md->size[1];
        create_3Dimage_ID("PFin_copy",
                          imgin.md->size[0],
                          imgin.md->size[1],
                          imgin.md->size[2],
                          &IDincp);

        inNBelem = imgin.md->size[0] * imgin.md->size[1] * imgin.md->size[2];
        break;

    default:
        printf("Invalid image size\n");
        break;
    }
    uint64_t xysize = (uint64_t) xsize * ysize;
    printf("xysize = %lu\n", xysize);


    /// Once input telemetry size measured, arrays are created:
    /// - pixarray_x  : x coordinate of each variable (useful to keep track of spatial coordinates)
    /// - pixarray_y  : y coordinate of each variable (useful to keep track of spatial coordinates)
    /// - pixarray_xy : combined index (avoids re-computing index frequently)
    /// - ave_inarray : time averaged value, useful because the predictive filter often needs average to be zero, so we will remove it

    long *pixarray_x = (long *) malloc(sizeof(long) * xsize * ysize);
    if (pixarray_x == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    long *pixarray_y = (long *) malloc(sizeof(long) * xsize * ysize);
    if (pixarray_y == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    long *pixarray_xy = (long *) malloc(sizeof(long) * xsize * ysize);
    if (pixarray_xy == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    double *ave_inarray = (double *) malloc(sizeof(double) * xsize * ysize);
    if (ave_inarray == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }


    /// ### Select input variables from mask (optional)
    /// If image "inmask" exists, use it to select which variables are active.
    /// Otherwise, all variables are active\n
    /// The number of active input variables is stored in NBpixin.

    imageID IDinmask = image_ID("inmask");
    long    NBpixin  = 0;
    if (IDinmask == -1)
    {
        for (uint32_t ii = 0; ii < xsize; ii++)
            for (uint32_t jj = 0; jj < ysize; jj++)
            {
                pixarray_x[NBpixin]  = ii;
                pixarray_y[NBpixin]  = jj;
                pixarray_xy[NBpixin] = jj * xsize + ii;
                NBpixin++;
            }
    }
    else
    {
        for (uint32_t ii = 0; ii < xsize; ii++)
            for (uint32_t jj = 0; jj < ysize; jj++)
                if (data.image[IDinmask].array.F[jj * xsize + ii] > 0.5)
                {
                    pixarray_x[NBpixin]  = ii;
                    pixarray_y[NBpixin]  = jj;
                    pixarray_xy[NBpixin] = jj * xsize + ii;
                    NBpixin++;
                }
    }
    printf("NBpixin = %ld\n", NBpixin);



    /// ## Selecting Output Variables
    /// By default, the output variables are the same as the input variables,
    /// so the prediction is performed on the same variables as the input.\n
    ///
    /// With inmask and outmask, input AND output variables can be
    /// selected amond the telemetry.

    /// Arrays are created:
    /// - outpixarray_x  : x coordinate of each output variable (useful to keep track of spatial coordinates)
    /// - outpixarray_y  : y coordinate of each output variable (useful to keep track of spatial coordinates)
    /// - outpixarray_xy : combined output index (avoids re-computing index frequently)

    long *outpixarray_x = (long *) malloc(sizeof(long) * xsize * ysize);
    if (outpixarray_x == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    long *outpixarray_y = (long *) malloc(sizeof(long) * xsize * ysize);
    if (outpixarray_y == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    long *outpixarray_xy = (long *) malloc(sizeof(long) * xsize * ysize);
    if (outpixarray_xy == NULL)
    {
        PRINT_ERROR("malloc returns NULL pointer");
        abort();
    }

    imageID IDoutmask = image_ID("outmask");
    long    NBpixout  = 0;
    if (IDoutmask == -1)
    {
        for (uint32_t ii = 0; ii < xsize; ii++)
            for (uint32_t jj = 0; jj < ysize; jj++)
            {
                outpixarray_x[NBpixout]  = ii;
                outpixarray_y[NBpixout]  = jj;
                outpixarray_xy[NBpixout] = jj * xsize + ii;
                NBpixout++;
            }
    }
    else
    {
        for (uint32_t ii = 0; ii < xsize; ii++)
            for (uint32_t jj = 0; jj < ysize; jj++)
                if (data.image[IDoutmask].array.F[jj * xsize + ii] > 0.5)
                {
                    outpixarray_x[NBpixout]  = ii;
                    outpixarray_y[NBpixout]  = jj;
                    outpixarray_xy[NBpixout] = jj * xsize + ii;
                    NBpixout++;
                }
    }




    /// ## Build Empty Data Matrix
    ///
    /// Note: column / row description follows FITS file viewing conventions.\n
    /// The data matrix is build from the telemetry. Each column (= time sample) of the
    /// data matrix consists of consecutives columns (= time sample) of the input telemetry.\n
    ///
    /// Variable naming:
    /// - NBmvec is the number of telemetry vectors (each corresponding to a different time) in the data matrix.
    /// - mvecsize is the size of each vector, equal to NBpixin times PForder
    ///
    /// Data matrix is stored as image of size NBmvec x mvecsize, to be fed to routine compute_SVDpseudoInverse in linopt_imtools (CPU mode) or in cudacomp (GPU mode)\n
    ///
    long NBmvec =
        nbspl - *PForder - (int) (*PFlatency) -
        2; // could put "-1", but "-2" allows user to change PFlag_run by up to 1 frame without reading out of array
    long mvecsize =
        NBpixin *
        *PForder; // size of each sample vector for AR filter, excluding regularization

    /// Regularization can be added to penalize strong coefficients in the predictive filter.
    /// It is optionally implemented by adding extra columns at the end of the data matrix.\n
    long    NBmvec1 = 0;
    imageID IDmatA  = -1;
    int     REG     = 0;
    if (REG == 0) // no regularization
    {
        printf("NBmvec   = %ld  -> %ld \n", NBmvec, NBmvec);
        NBmvec1 = NBmvec;
        create_2Dimage_ID("PFmatD", NBmvec, mvecsize, &IDmatA);
    }
    else // with regularization
    {
        printf("NBmvec   = %ld  -> %ld \n", NBmvec, NBmvec + mvecsize);
        NBmvec1 = NBmvec + mvecsize;
        create_2Dimage_ID("PFmatD", NBmvec + mvecsize, mvecsize, &IDmatA);
    }



    /// Data matrix conventions :
    /// - each column (ii = cst) is a measurement
    /// - m index is measurement
    /// - dt*NBpixin+pix index is pixel

    printf("mvecsize = %ld  (%u x %ld)\n", mvecsize, *PForder, NBpixin);
    printf("NBpixin = %ld\n", NBpixin);
    printf("NBpixout = %ld\n", NBpixout);
    printf("NBmvec1 = %ld\n", NBmvec1);
    printf("PForder = %u\n", *PForder);

    printf("xysize = %ld\n", xysize);
    printf("IDin = %ld\n\n", imgin.ID);
    list_image_ID();




    INSERT_STD_PROCINFO_COMPUTEFUNC_START


    printf("=========== LOOP ITERATION %6ld =======\n", processinfo->loopcnt);
    printf("  PFlag     = %20f      ", *PFlatency);
    printf("  SVDeps    = %20f\n", *SVDeps);
    printf("  RegLambda = %20f      ", *reglambda);
    printf("  LOOPgain  = %20f\n", *loopgain);
    printf("\n");




    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(pixarray_x);
    free(pixarray_y);
    free(pixarray_xy);

    free(outpixarray_x);
    free(outpixarray_y);
    free(outpixarray_xy);

    DEBUG_TRACE_FEXIT();
    return RETURN_SUCCESS;
}




INSERT_STD_FPSCLIfunctions



    // Register function in CLI
    errno_t
    CLIADDCMD_LinARfilterPred__build_linPF()
{

    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    CLIcmddata.FPS_customCONFcheck = customCONFcheck;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
