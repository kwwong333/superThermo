{

    bool closedVolume = p_rgh.needReference();

    dimensionedScalar compressibility = fvc::domainIntegrate(psi);
    bool compressible = (compressibility.value() > SMALL);

    rho = thermo.rho();

    // Thermodynamic density needs to be updated by psi*d(p) after the
    // pressure solution - done in 2 parts. Part 1:
    thermo.rho() -= psi*p_rgh;

    volScalarField rAU(1.0/UEqn.A());
    surfaceScalarField rhorAUf("rhorAUf", fvc::interpolate(rho*rAU));
    volVectorField HbyA(constrainHbyA(rAU*UEqn.H(), U, p_rgh));

    surfaceScalarField phig(-rhorAUf*ghf*fvc::snGrad(rho)*mesh.magSf());

    surfaceScalarField phiHbyA
    (
        "phiHbyA",
        (
            fvc::flux(rho*HbyA)
          + rhorAUf*fvc::ddtCorr(rho, U, phi)
        )
      + phig
    );

    MRF.makeRelative(fvc::interpolate(rho), phiHbyA);

    // Update the pressure BCs to ensure flux consistency
    constrainPressure(p_rgh, rho, U, phiHbyA, rhorAUf, MRF);

    tmp<fvScalarMatrix> p_rghDDtEqn
    (
        new fvScalarMatrix(p_rgh, dimMass/dimTime)
    );

    if (compressible)
    {
        p_rghDDtEqn =
        (
            fvc::ddt(rho) + psi*correction(fvm::ddt(p_rgh))
        );
    }



    while (pimple.correctNonOrthogonal())
    {
        fvScalarMatrix p_rghEqn
        (
            p_rghDDtEqn()
          + fvc::div(phiHbyA)
          - fvm::laplacian(rhorAUf, p_rgh)
        ==
            fvOptions(psi, p_rgh, rho.name())
        );

        p_rghEqn.solve(mesh.solver(p_rgh.select(pimple.finalInnerIter())));

        if (pimple.finalNonOrthogonalIter())
        {
            // Calculate the conservative fluxes
            phi = phiHbyA + p_rghEqn.flux();

            // Explicitly relax pressure for momentum corrector
            p_rgh.relax();

            // Correct the momentum source with the pressure gradient flux
            // calculated from the relaxed pressure
               U = HbyA + rAU*fvc::reconstruct((phig + p_rghEqn.flux())/rhorAUf);
               U.correctBoundaryConditions();

            //fvOptions.correct(U);
            // Added by MN
		  
                   rAGave = 0.0;
                   magGbarAve = 0.0;

                   rAGave = gSum( ( 1.0/UEqn.A() ) * rho.field() * mesh.V() ) / gSum( mesh.V() );

	           GbarAve = gSum( rho.field() * U.field() * mesh.V() ) / gSum( mesh.V() );
		   magGbarAve = mag(GbarAve);

		   dGradP = GRelax*(mag(Gbar) - magGbarAve)/rAGave;

		   //correct U
                   scalarField rAU2 = ( 1.0/UEqn.A() * mesh.V() ) / ( mesh.V());
		   U.field() += flowDir*rAU2*dGradP;
		   U.correctBoundaryConditions();

		   gradP = gradP0 + dGradP;   
		    
	           dimensionedVector SuUpdate("SuUpdate", dimensionSet( 1, -2, -2, 0, 0, 0, 0 ), flowDir*gradP ); 
		   Su = SuUpdate;  

            K = 0.5*magSqr(U);
        }
    }

    p = p_rgh + rho*gh;

    // Second part of thermodynamic density update
    thermo.rho() += psi*p_rgh;

    if (thermo.dpdt())
    {
        dpdt = fvc::ddt(p);
    }

    if (compressible)
    {
        #include "rhoEqn.H"
    }
    #include "compressibleContinuityErrs.H"

    if (closedVolume)
    {
        if (!compressible)
        {
            p += dimensionedScalar
            (
                "p",
                p.dimensions(),
                pRefValue - getRefCellValue(p, pRefCell)
            );
        }
        else
        {
            p += (initialMass - fvc::domainIntegrate(thermo.rho()))
                /compressibility;
            rho = thermo.rho();
        }
        p_rgh = p - rho*gh;
    }

    Info<< " rho max/min : " << max(rho).value() << " " << min(rho).value()
        << endl;
}
