/*============================================================================*\
Solves Navier-Stokes equations for incompressible, laminar, steady flow using
artificial compressibility method on staggered grid.

The governing equations are as follows:
P_t + c^2 div[u] = 0
u_t + u . grad[u] = - grad[P] + nu div[grad[u]]

where P is p/rho and c represents artificial sound's speed.

Lid-Driven Cavity case:
Dimensions : 1x1 m
Grid size  : 128 x 128
Re number  : 100 / 1000 / 5000 / 10000
Grid type  : Staggered Arakawa C
Boundary Conditions: u, v -> Dirichlet (as shown below)
                     p    -> Neumann (grad[p] = 0)

                                u=1, v=0
                             ---------------
                            |               |
                        u=0 |               | u=0
                        v=0 |               | v=0
                            |               |
                            |               |
                             ---------------
                                  u=0, v=0
\*============================================================================*/
#include "functions.h"

/* Specify number of grid points in x and y directions */
#define IX 128
#define IY 128

/* Main program */
int main (int argc, char *argv[])
{
  /* Two arrays are required for each Variable; one for old time step and one
   * for the new time step. */
  double **ubufo, **ubufn, **u, **un, **utmp;
  double **vbufo, **vbufn, **v, **vn, **vtmp;
  double **pbufo, **pbufn, **p, **pn, **ptmp;

  double dx, dy, dt, Re, nu;
  double dtdx, dtdy, dtdxx, dtdyy, dtdxdy;

  int i, j, itr = 1, itr_max = 1e6;
  double ut, ub, ul, ur;
  double vt, vb, vl, vr;
  double err_tot, err_u, err_v, err_p, err_d;

  /* Define first and last interior nodes number for better
   * code readability*/
  const int xlo = 1, xhi = IX - 1, xtot = IX + 1, xm = IX/2;
  const int ylo = 1, yhi = IY - 1, ytot = IY + 1, ym = IY/2;

  const double tol = 1.0e-7, l_lid = 1.0, gradp = 0.0;

  /* Simulation parameters */
  double cfl , c2;

  /* Getting Reynolds number */
  if(argc <= 1) {
    Re = 100.0;
  } else {
    char *ptr;
    Re = strtod(argv[1], &ptr);
  }
  printf("Re number is set to %d\n", (int) Re);

  if (Re < 500) {
      cfl = 0.15;
      c2 = 5.0;
  } else if (Re < 2000) {
      cfl = 0.20;
      c2 = 5.8;
  } else {
      cfl = 0.05;
      c2 = 5.8;
  }

  /* Boundary condition values */
  ut = 1.0;
  ub = ul = ur = 0.0;
  vt = vb = vl = vr = 0.0;

  /* Create a log file for outputting the residuals */
  FILE *flog;
  flog = fopen("data/residual","w+t");

  /* Compute flow parameters based on inputs */
  dx = l_lid / (double) (IX - 1);
  dy = dx;
  dt = cfl * fmin(dx,dy) / ut;
  nu = ut * l_lid / Re;

  /* Carry out operations that their values do not change in loops */
  dtdx = dt / dx;
  dtdy = dt / dy;
  dtdxx = dt / (dx * dx);
  dtdyy = dt / (dy * dy);
  dtdxdy = dt * dx * dy;

  /* Generate two 2D arrays for storing old and new velocity field
   * in x-direction */
  ubufo = array_2d(IX, ytot);
  ubufn = array_2d(IX, ytot);
  /* Define two pointers to the generated buffers for velocity field
   * in x-direction */
  u = ubufo;
  un = ubufn;

  /* Generate two 2D arrays for storing old and new velocity field
   * in y-direction */
  vbufo = array_2d(xtot, IY);
  vbufn = array_2d(xtot, IY);
  /* Define two pointers to the generated buffers for velocity field
   * in y-direction */
  v = vbufo;
  vn = vbufn;

  /* Generate two 2D arrays for storing old and new pressure field*/
  pbufo = array_2d(xtot, ytot);
  pbufn = array_2d(xtot, ytot);
  /* Define two pointers to the generated buffers for pressure field*/
  p = pbufo;
  pn = pbufn;

  /* Apply initial conditions*/
  for (i = xlo; i < xhi; i++) {
    u[i][ytot - 1] = ut;
    u[i][ytot - 2] = ut;
  }

  /* Applying boundary conditions */
  /* Dirichlet boundary condition */
  for (i = xlo; i < xhi; i++) {
    u[i][0] = ub - u[i][1];
    u[i][ytot - 1] = 2.0*ut - u[i][ytot - 2];
  }
  for (j = 0; j < ytot; j++) {
    u[0][j] = ul;
    u[xhi][j] = ur;
  }

  /* Dirichlet boundary condition */
  for (i = xlo; i < xtot - 1; i++) {
    v[i][0] = vb;
    v[i][yhi] = vt;
  }
  for (j = 0; j < IY; j++) {
    v[0][j] = vl - v[1][j];
    v[xtot - 1][j] = vr - v[xtot - 2][j];
  }

  /* Neumann boundary condition */
  for (i = xlo; i < xtot - 1; i++) {
    p[i][0] = p[i][1] - dy * gradp;
    p[i][ytot - 1] = p[i][ytot - 2]  - dy * gradp;
  }
  for (j = 0; j < ytot; j++) {
    p[0][j] = p[1][j] - dx * gradp;
    p[xtot - 1][j] = p[xtot - 2][j] - dx * gradp;
  }

  /* Start the main loop */
  do {
    /* Solve x-momentum equation for computing u */
    #pragma omp parallel for private(i,j) schedule(auto)
    for (i = xlo; i < xhi; i++) {
      for (j = ylo; j < ytot - 1; j++) {
        un[i][j] = u[i][j]
                   - 0.25*dtdx * (pow(u[i+1][j] + u[i][j], 2)
                                  - pow(u[i][j] + u[i-1][j], 2)) \
                   - 0.25*dtdy * ((u[i][j+1] + u[i][j])
                                  * (v[i+1][j] + v[i][j])
                                  - (u[i][j] + u[i][j-1])
                                  * (v[i+1][j-1] + v[i][j-1])) \
                   - dtdx * (p[i+1][j] - p[i][j])
                   + nu * (dtdxx * (u[i+1][j] - 2.0 * u[i][j] + u[i-1][j])
                           + dtdyy * (u[i][j+1] - 2.0 * u[i][j] + u[i][j-1]));
      }
    }

    /* Solve y-momentum for computing v */
    #pragma omp parallel for private(i,j) schedule(auto)
    for (i = xlo; i < xtot - 1; i++) {
      for (j = ylo; j < yhi; j++) {
        vn[i][j] = v[i][j]
                   - 0.25*dtdx * ((u[i][j+1] + u[i][j])
                                  * (v[i+1][j] + v[i][j])
                                  - (u[i-1][j+1] + u[i-1][j])
                                  * (v[i][j] + v[i-1][j])) \
                   - 0.25*dtdy * (pow(v[i][j+1] + v[i][j], 2)
                                  - pow(v[i][j] + v[i][j-1], 2)) \
                   - dtdy * (p[i][j+1] - p[i][j])
                   + nu * (dtdxx * (v[i+1][j] - 2.0 * v[i][j] + v[i-1][j])
                           + dtdyy * (v[i][j+1] - 2.0 * v[i][j] + v[i][j-1]));
      }
    }

    /* Dirichlet boundary conditions */
    for (i = xlo; i < xhi; i++) {
      un[i][0] = ub - un[i][1];
      un[i][ytot - 1] = 2.0*ut - un[i][ytot - 2];
    }
    for (j = 0; j < ytot; j++) {
      un[0][j] = ul;
      un[xhi][j] = ur;
    }

    /* Dirichlet boundary conditions */
    for (i = xlo; i < xtot - 1; i++) {
      vn[i][0] = vb;
      vn[i][yhi] = vt;
    }
    for (j = 0; j < IY; j++) {
      vn[0][j] = vr - vn[1][j];
      vn[xtot - 1][j] = vl - vn[xtot - 2][j];
    }

    /* Solves continuity equation for computing P */
    #pragma omp parallel for private(i,j) schedule(auto)
    for (i = xlo; i < xtot - 1; i++) {
      for (j = ylo; j < ytot - 1; j++) {
        pn[i][j] = p[i][j] - c2 * ((un[i][j] - un[i-1][j]) * dtdx
                                   + (vn[i][j] - vn[i][j-1]) * dtdy);
      }
    }

    /* Neumann boundary conditions */
    for (i = xlo; i < xtot - 1; i++) {
      pn[i][0] = pn[i][1] - dy * gradp;
      pn[i][ytot - 1] = pn[i][ytot - 2]  - dy * gradp;
    }
    for (j = 0; j < ytot; j++) {
      pn[0][j] = pn[1][j] - dx * gradp;
      pn[xtot - 1][j] = pn[xtot - 2][j] - dx * gradp;
    }

    /* Compute L2-norm */
    err_u = err_v = err_p = err_d = 0.0;
    #pragma omp parallel for private(i,j) schedule(auto) \
                             reduction(+:err_u, err_v, err_p, err_d)
    for (i = xlo; i < xhi; i++) {
      for (j = ylo; j < yhi; j++) {
        err_u += pow(un[i][j] - u[i][j], 2);
        err_v += pow(vn[i][j] - v[i][j], 2);
        err_p += pow(pn[i][j] - p[i][j], 2);
        err_d += (un[i][j] - un[i-1][j]) * dtdx
                 + (vn[i][j] - vn[i][j-1]) * dtdy;
      }
    }

    err_u = sqrt(dtdxdy * err_u);
    err_v = sqrt(dtdxdy * err_v);
    err_p = sqrt(dtdxdy * err_p);

    err_tot = fmaxof(err_u, err_v, err_p, err_d);

    /* Check if solution diverged */
    if (isnan(err_tot)) {
      printf("Solution Diverged after %d iterations!\n", itr);

      /* Free the memory and terminate */
      freeMem(ubufo, vbufo, pbufo, ubufn, vbufn, pbufn);
      exit(EXIT_FAILURE);
    }

    /* Write relative error */
    fprintf(flog ,"%d \t %.8lf \t %.8lf \t %.8lf \t %.8lf \t %.8lf\n",
            itr, err_tot, err_u, err_v, err_p, err_d);

    /* Changing pointers to point to the newly computed fields */
    utmp = u;
    u = un;
    un = utmp;

    vtmp = v;
    v = vn;
    vn = vtmp;

    ptmp = p;
    p = pn;
    pn = ptmp;

    itr += 1;
  } while (err_tot > tol && itr < itr_max);

  if (itr == itr_max) {
    printf("Maximum number of iterations, %d, exceeded\n", itr);

    /* Free the memory and terminate */
    freeMem(ubufo, vbufo, pbufo, ubufn, vbufn, pbufn);

    exit(EXIT_FAILURE);
  }

  printf("Converged after %d iterations\n", itr);
  fclose(flog);

  /* Computing new arrays for computing the fields along lines crossing
     the center of the domain in x- and y-directions */
  double **u_g, **v_g, **p_g;
  v_g = array_2d(IX, IY);
  u_g = array_2d(IX, IY);
  p_g = array_2d(IX, IY);

  #pragma omp parallel for private(i,j) schedule(auto)
  for (i = 0; i < IX; i++) {
   for (j = 0; j < IY; j++) {
     u_g[i][j] = 0.5 * (u[i][j+1] + u[i][j]);
     v_g[i][j] = 0.5 * (v[i+1][j] + v[i][j]);
     p_g[i][j] = 0.25 * (p[i][j] + p[i + 1][j] + p[i][j + 1] + p[i + 1][j + 1]);
   }
  }

  /* Free the memory */
  freeMem(ubufo, vbufo, pbufo, ubufn, vbufn, pbufn);

  /* Writing fields data for post-processing */
  FILE *fug, *fvg, *fd;

  /* Velocity field value along a line crossing the middle of x-axis */
  fug = fopen("data/Central_U","w+t");
  fprintf(fug, "# U, Y\n");

  for (j = 0; j < IY; j++) {
    fprintf(fug, "%.8lf \t %.8lf\n", 0.5 * (u_g[xm][j] + u_g[xm + 1][j]),
            (double) j*dy );
  }

  fclose(fug);

  /* Velocity field value along a line crossing the middle of y-axis */
  fvg = fopen("data/Central_V","w+t");
  fprintf(fug, "# V, X\n");

  for (i = 0; i < IX; i++) {
    fprintf(fvg, "%.8lf \t %.8lf\n", 0.5 * (v_g[i][ym] + v_g[i][ym + 1]),
            (double) i*dx );
  }

  fclose(fvg);

  /* Writing all the field data */
  fd = fopen("data/xyuvp","w+t");
  fprintf(fd, "# X \t Y \t U \t V \t P\n");
  for (i = 0; i < IX; i++) {
    for (j = 0; j < IY; j++) {
       fprintf(fd, "%.8lf \t %.8lf \t %.8lf \t %.8lf \t %.8lf\n",
               (double) i*dx, (double) j*dy, u_g[i][j], v_g[i][j], p_g[i][j]);
    }
  }

  fclose(fd);

  /* Free the memory */
  freeMem(u_g, v_g, p_g);

  return 0;
}