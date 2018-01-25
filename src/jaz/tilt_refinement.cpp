#include <src/jaz/tilt_refinement.h>
#include <src/jaz/gravis/t2Matrix.h>
#include <src/jaz/gravis/t4Matrix.h>

using namespace gravis;

void TiltRefinement::updateTiltShift(
        const Image<Complex> &prediction,
        const Image<Complex> &observation,
        CTF &ctf, RFLOAT angpix,
        Image<Complex>& xyDest,
        Image<RFLOAT>& wDest)
{

    const long w = prediction.data.xdim;
    const long h = prediction.data.ydim;

    const RFLOAT as = (RFLOAT)h * angpix;

    for (long y = 0; y < h; y++)
    for (long x = 0; x < w; x++)
    {
        const double xf = x;
        const double yf = y < w? y : y - h;

        Complex vx = DIRECT_A2D_ELEM(prediction.data, y, x);
        Complex vy = DIRECT_A2D_ELEM(observation.data, y, x);

        RFLOAT c = ctf.getCTF(xf/as, yf/as);

        DIRECT_A2D_ELEM(xyDest.data, y, x) += c * vx.conj() * vy;
        DIRECT_A2D_ELEM(wDest.data, y, x) += c * c * vx.norm();
    }
}

void TiltRefinement::updateTiltShiftPar(
        const Image<Complex> &prediction,
        const Image<Complex> &observation,
        CTF &ctf, RFLOAT angpix,
        Image<Complex>& xyDest,
        Image<RFLOAT>& wDest)
{
    const long w = prediction.data.xdim;
    const long h = prediction.data.ydim;

    const RFLOAT as = (RFLOAT)h * angpix;

    #pragma omp parallel for
    for (long y = 0; y < h; y++)
    for (long x = 0; x < w; x++)
    {
        const double xf = x;
        const double yf = y < w? y : y - h;

        Complex vx = DIRECT_A2D_ELEM(prediction.data, y, x);
        Complex vy = DIRECT_A2D_ELEM(observation.data, y, x);

        RFLOAT c = ctf.getCTF(xf/as, yf/as);

        DIRECT_A2D_ELEM(xyDest.data, y, x) += c * vx.conj() * vy;
        DIRECT_A2D_ELEM(wDest.data, y, x) += c * c * vx.norm();
    }
}

void TiltRefinement::fitTiltShift(const Image<RFLOAT>& phase,
                             const Image<RFLOAT>& weight,
                             RFLOAT Cs, RFLOAT lambda, RFLOAT angpix,
                             RFLOAT* shift_x, RFLOAT* shift_y,
                             RFLOAT* tilt_x, RFLOAT* tilt_y,
                             Image<RFLOAT>* fit,
                             d2Matrix magCorr)
{
    const long w = phase.data.xdim;
    const long h = phase.data.ydim;

    double axx = 0.0, axy = 0.0, axz = 0.0,//axw == ayz,
                      ayy = 0.0, ayz = 0.0, ayw = 0.0,
                                 azz = 0.0, azw = 0.0,
                                            aww = 0.0;

    double bx = 0.0, by = 0.0, bz = 0.0, bw = 0.0;

    const RFLOAT as = (RFLOAT)h * angpix;

    for (long yi = 0; yi < h; yi++)
    for (long xi = 0; xi < w; xi++)
    {
        double x = xi;
        double y = yi < w? yi : ((yi-h));

        d2Vector p = magCorr * d2Vector(x,y);
        x = p.x/as;
        y = p.y/as;

        double q = x*x + y*y;

        double v = DIRECT_A2D_ELEM(phase.data, yi, xi);
        double g = DIRECT_A2D_ELEM(weight.data, yi, xi);

        axx += g     * x * x;
        axy += g     * x * y;
        axz += g * q * x * x;

        ayy += g     * y * y;
        ayz += g * q * x * y;
        ayw += g * q * y * y;

        azz += g * q * q * x * x;
        azw += g * q * q * x * y;

        aww += g * q * q * y * y;

        bx += g * x * v;
        by += g * y * v;
        bz += g * q * x * v;
        bw += g * q * y * v;
    }

    gravis::d4Matrix A;
    gravis::d4Vector b(bx, by, bz, bw);

    A(0,0) = axx;
    A(0,1) = axy;
    A(0,2) = axz;
    A(0,3) = ayz;

    A(1,0) = axy;
    A(1,1) = ayy;
    A(1,2) = ayz;
    A(1,3) = ayw;

    A(2,0) = axz;
    A(2,1) = ayz;
    A(2,2) = azz;
    A(2,3) = azw;

    A(3,0) = ayz;
    A(3,1) = ayw;
    A(3,2) = azw;
    A(3,3) = aww;

    gravis::d4Matrix Ainv = A;
    Ainv.invert();

    gravis::d4Vector opt = Ainv * b;

    std::cout << opt[0] << ", " << opt[1] << ", " << opt[2] << ", " << opt[3] << "\n";

    *shift_x = opt[0];
    *shift_y = opt[1];

    *tilt_x = -opt[2]*180.0/(0.360 * Cs * 10000000 * lambda * lambda * 3.141592654);
    *tilt_y = -opt[3]*180.0/(0.360 * Cs * 10000000 * lambda * lambda * 3.141592654);

    /*destA = gravis::d2Vector(opt[0], opt[1]).length();
    destPhiA = (180.0/3.1416)*std::atan2(opt[1], opt[0]);

    destB = gravis::d2Vector(opt[2], opt[3]).length();
    destPhiB = (180.0/3.1416)*std::atan2(opt[3], opt[2]);

    std::cout << "linear: " << destA << " @ " << destPhiA << "°\n";
    std::cout << "cubic:  " << destB << " @ " << destPhiB << "°\n";
    std::cout << "    =  -" << destB << " @ " << (destPhiB + 180.0) << "°\n";*/

    std::cout << "tilt_x = " << *tilt_x << "\n";
    std::cout << "tilt_y = " << *tilt_y << "\n";

    if (fit != 0)
    {
        *fit = Image<RFLOAT>(w,h);

        for (long yi = 0; yi < h; yi++)
        for (long xi = 0; xi < w; xi++)
        {
            double x = xi;
            double y = yi < w? yi : ((yi-h));

            d2Vector p = magCorr * d2Vector(x,y);
            x = p.x/as;
            y = p.y/as;

            double q = x*x + y*y;

            DIRECT_A2D_ELEM(fit->data, yi, xi) = x * opt[0] + y * opt[1] + q * x * opt[2] + q * y * opt[3];
        }
    }
}
