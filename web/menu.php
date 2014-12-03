<div class="ui top menu" id="id_topMenu">
  <div class="top wrapper">
    <a class="item" id="id_logo_div" href="/index.php">
      <img id="id_top_logo"    class="top logo"    src="/images/logo/blueOccaLogo.png"/>
      <img id="id_bottom_logo" class="bottom logo" src="/images/logo/blackOccaLogo.png"/>
    </a>

    <div class="right menu">
      <a class="light topMenu black item <?php addSelected('downloads') ?>"
         href="/downloads.php">
        Downloads
      </a>

	    <div class="ui light topMenu black simple dropdown item <?php addSelected('getStarted') ?>">
        <a class="topMenu" href="/getStarted.php">
          Get Started
        </a>
        <div class="menu">
          <a class="item" href="/getStarted.php#Linux"  > Linux    </a>
          <a class="item" href="/getStarted.php#MacOSX" > Mac OS X </a>
          <a class="item" href="/getStarted.php#Windows"> Windows  </a>
        </div>
      </div>

      <a class="light topMenu black item <?php addSelected('tutorials') ?>"
         href="/tutorials.php">
        Tutorials
      </a>

	    <div class="ui light topMenu black simple dropdown item <?php addSelected('documentation') ?>">
        <a class="topMenu" href="/documentation.php">
          Documentation
        </a>
        <div class="menu">
          <div class="ui dropdown item">
            <a class="noDecor" href="/documentation.php#Host-API">Host API <i class="dropdown icon"></i></a>
            <div class="menu">
              <a class="item" href="/documentation/hostAPI/C.php"      > C       </a>
              <a class="item" href="/documentation/hostAPI/CPP.php"    > C++     </a>
              <a class="item" href="/documentation/hostAPI/CS.php"     > C#      </a>
              <a class="item" href="/documentation/hostAPI/Fortran.php"> Fortran </a>
              <a class="item" href="/documentation/hostAPI/Julia.php"  > Julia   </a>
              <a class="item" href="/documentation/hostAPI/Python.php" > Python  </a>
              <a class="item" href="/documentation/hostAPI/Matlab.php" > MATLAB  </a>
            </div>
          </div>
          <div class="ui dropdown item">
            <a class="noDecor" href="/documentation.php#Device-API">Device API <i class="dropdown icon"></i></a>
            <div class="menu">
              <a class="item" href="/documentation/deviceAPI/OKL.php"> OKL </a>
              <a class="item" href="/documentation/deviceAPI/OFL.php"> OFL </a>
            </div>
          </div>
        </div>
      </div>

	    <a class="light topMenu black item <?php addSelected('aboutUs') ?>"
         href="/aboutUs.php">
        About Us
      </a>
    </div> <!--[ right menu ]-->
  </div> <!--[ top wrapper ]-->
</div> <!--[ id_topMenu ]-->