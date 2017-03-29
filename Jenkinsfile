properties ([[$class: 'ParametersDefinitionProperty', parameterDefinitions: [
  [$class: 'StringParameterDefinition', name: 'mbed_os_revision', defaultValue: 'mbed-os-5.4', description: 'Revision of mbed-os to build'],
  [$class: 'BooleanParameterDefinition', name: 'smoke_test', defaultValue: true, description: 'Runs HW smoke tests on Cellular devices']
  ]]])

echo "Run smoke tests: ${params.smoke_test}"

try {
  echo "Verifying build with mbed-os version ${mbed_os_revision}"
  env.MBED_OS_REVISION = "${mbed_os_revision}"
} catch (err) {
  def mbed_os_revision = "master"
  echo "Verifying build with mbed-os version ${mbed_os_revision}"
  env.MBED_OS_REVISION = "${mbed_os_revision}"
}

// Map RaaS instances to corresponding test suites
def raas = [
  "cellular_minimal_smoke_ublox_c027.json": "8072",
  "cellular_minimal_smoke_mts_dragonfly.json": "8072"
  ]

// List of targets with supported modem families
def target_families = [
  "UBLOX": ["UBLOX_C027"],
  "MTS_DRAGONFLY": ["MTS_DRAGONFLY_F411RE"]
  ]

// Supported Modems
def targets = [
  "UBLOX_C027",
  "UBLOX_C030",
  "MTS_DRAGONFLY_F411RE"
]

// Map toolchains to compilers
def toolchains = [
  ARM: "armcc",
  GCC_ARM: "arm-none-eabi-gcc"//,
  //IAR: "iar_arm"
  ]

def stepsForParallel = [:]

// Jenkins pipeline does not support map.each, we need to use oldschool for loop
for (int i = 0; i < target_families.size(); i++) {
  for(int j = 0; j < toolchains.size(); j++) {
    for(int k = 0; k < targets.size(); k++) {
      def target_family = target_families.keySet().asList().get(i)
      def allowed_target_type = target_families.get(target_family)
      def target = targets.get(k)
      def toolchain = toolchains.keySet().asList().get(j)
      def compilerLabel = toolchains.get(toolchain)

      def stepName = "${target} ${toolchain}"
      if(allowed_target_type.contains(target)) {
        stepsForParallel[stepName] = buildStep(target_family, target, compilerLabel, toolchain)
      }
    }
  }
}


def parallelRunSmoke = [:]

// Need to compare boolean against string value
if ( params.smoke_test == true ) {
  // Generate smoke tests based on suite amount
  for(int i = 0; i < raas.size(); i++) {
    def suite_to_run = raas.keySet().asList().get(i)
    def raasPort = raas.get(suite_to_run)
    // Parallel execution needs unique step names. Remove .json file ending.
    def smokeStep = "${raasPort} ${suite_to_run.substring(0, suite_to_run.indexOf('.'))}"
    parallelRunSmoke[smokeStep] = run_smoke(target_families, raasPort, suite_to_run, toolchains, targets)
  }
}

timestamps {
  parallel stepsForParallel
  parallel parallelRunSmoke
}

def buildStep(target_family, target, compilerLabel, toolchain) {
  return {
    stage ("${target_family}_${target}_${compilerLabel}") {
      node ("${compilerLabel}") {
        deleteDir()
        dir("mbed-os-example-cellular-minimal") {
          checkout scm
          def config_file = "mbed_app.json"

           // Change target type
          execute("sed -i 's/\"platform\": .*/\"platform\": \"${target_family}\"/' ${config_file}")

          // Activate traces
          execute("sed -i 's/\"mbed-trace.enable\": false/\"mbed-trace.enable\": true/' ${config_file}")

          // Set mbed-os to revision received as parameter
          execute ("mbed deploy --protocol ssh")
          //dir ("mbed-os") {
          //  execute ("git checkout ${env.MBED_OS_REVISION}")
          //}

          execute ("mbed compile --build out/${target}_${toolchain}/ -m ${target} -t ${toolchain} -c --app-config ${config_file}")
        }
        stash name: "${target}_${toolchain}", includes: '**/mbed-os-example-cellular-minimal.bin'
        archive '**/mbed-os-example-cellular-minimal.bin'
        step([$class: 'WsCleanup'])
      }
    }
  }
}

def run_smoke(target_families, raasPort, suite_to_run, toolchains, targets) {
  return {
    // Remove .json from suite name
    def suiteName = suite_to_run.substring(0, suite_to_run.indexOf('.'))
    stage ("smoke_${raasPort}_${suiteName}") {
      //node is actually the type of machine, i.e., mesh-test boild down to linux
      node ("mesh-test") {
        deleteDir()
        dir("mbed-clitest") {
          git "git@github.com:ARMmbed/mbed-clitest.git"
          execute("git checkout master")
          execute("git submodule update --init --recursive testcases")
          
          dir("testcases") {
            execute("git all checkout master")
            execute("git submodule update --init --recursive cellular")
            execute("git all checkout master")
          }
        
    for (int i = 0; i < target_families.size(); i++) {
      for(int j = 0; j < toolchains.size(); j++) {
        for(int k = 0; k < targets.size(); k++) {
            def target_family = target_families.keySet().asList().get(i)
            def allowed_target_type = target_families.get(target_family)
            def target = targets.get(k)
            def toolchain = toolchains.keySet().asList().get(j)

            if(allowed_target_type.contains(target)) {
              unstash "${target}_${toolchain}"
            }
        }
      }
    }

          env.RAAS_USERNAME = "user"
          env.RAAS_PASSWORD = "user"
          if (target == "MTS_DRAGONFLY_F411RE")  {
            execute("python clitest.py --suitedir testcases/suites/ --suite ${suite_to_run} --type hardware --reset hard --raas 193.208.80.31:${raasPort} --tcdir testcases/cellular  --failure_return_value -vvv -w --log log_${raasPort}_${suiteName}")
          }
          else {
            execute("python clitest.py --suitedir testcases/suites/ --suite ${suite_to_run} --type hardware --reset --raas 193.208.80.31:${raasPort} --tcdir testcases/cellular  --failure_return_value -vvv -w --log log_${raasPort}_${suiteName}")
          }
         archive "log_${raasPort}_${suiteName}/**/*"
        }
      }
    }
  }
}