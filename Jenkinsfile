properties ([[$class: 'ParametersDefinitionProperty', parameterDefinitions: [
  [$class: 'StringParameterDefinition', name: 'mbed_os_revision', defaultValue: 'mbed-os-5.4', description: 'Revision of mbed-os to build'],
  [$class: 'BooleanParameterDefinition', name: 'smoke_test', defaultValue: false, description: 'Runs HW smoke tests on Cellular devices']
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
  "cellular_minimal_smoke_ublox.json": "8072"
  ]

// List of targets with supported modem families
def targets = [
  "UBLOX": ["UBLOX_C027", "UBLOX_C027"],
  "MTS_DRAGONFLY": ["MTS_DRAGONFLY"]
  ]

// Supported Modems
def modems = [
  "UBLOX_C027",
  "UBLOX_C027",
  "MTS_DRAGONFLY"
]

// Map toolchains to compilers
def toolchains = [
  ARM: "armcc",
  GCC_ARM: "arm-none-eabi-gcc",
  IAR: "iar_arm"
  ]

def stepsForParallel = [:]

// Jenkins pipeline does not support map.each, we need to use oldschool for loop
for (int i = 0; i < targets.size(); i++) {
  for(int j = 0; j < toolchains.size(); j++) {
      def target = targets.keySet().asList().get(i)
      def allowed_modem_type = targets.get(target)
      def toolchain = toolchains.keySet().asList().get(j)
      def compilerLabel = toolchains.get(toolchain)

      // Skip unwanted combination
      if (target == "NUCLEO_F401RE" && toolchain == "IAR") {
        continue
      }

      def stepName = "${target} ${toolchain}"
      if(allowed_modem_type.contains(modems)) {
        stepsForParallel[stepName] = buildStep(target, compilerLabel, toolchain)
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
    parallelRunSmoke[smokeStep] = run_smoke(targets, toolchains, raasPort, suite_to_run, modems)
  }
}

timestamps {
  parallel stepsForParallel
  parallel parallelRunSmoke
}

def buildStep(target, compilerLabel, toolchain) {
  return {
    stage ("${target}_${compilerLabel}") {
      node ("${compilerLabel}") {
        deleteDir()
        dir("mbed-os-example-cellular-minimal") {
          checkout scm
          def config_file = "mbed_app.json"

          // Activate traces
          execute("sed -i 's/\"mbed-trace.enable\": false/\"mbed-trace.enable\": true/' ${config_file}")

          // Set mbed-os to revision received as parameter
          execute ("mbed deploy --protocol ssh")
          dir ("mbed-os") {
            execute ("git checkout ${env.MBED_OS_REVISION}")
          }

          execute ("mbed compile --build out/${target}_${toolchain}/ -m ${target} -t ${toolchain} -c --app-config ${config_file}")
        }
        stash name: "${target}_${toolchain}", includes: '**/mbed-os-example-cellular-minimal.bin'
        archive '**/mbed-os-example-cellular-minimal.bin'
        step([$class: 'WsCleanup'])
      }
    }
  }
}

def run_smoke(targets, toolchains, raasPort, suite_to_run, modems) {
  return {
    // Remove .json from suite name
    def suiteName = suite_to_run.substring(0, suite_to_run.indexOf('.'))
    stage ("smoke_${raasPort}_${suiteName}") {
      node ("cellular-test") {
        deleteDir()
        dir("mbed-clitest") {
          git "git@github.com:ARMmbed/mbed-clitest.git"
          execute("git checkout ${env.LATEST_CLITEST_REL}")
          execute("git submodule update --init --recursive testcases")

          dir("testcases") {
            execute("git checkout master")
            dir("6lowpan") {
              execute("git checkout master")
            }
          }

          for (int i = 0; i < targets.size(); i++) {
            for(int j = 0; j < toolchains.size(); j++) {
              def target = targets.keySet().asList().get(i)
              def allowed_modems = targets.get(target)
              def toolchain = toolchains.keySet().asList().get(j)
            }
          }
         
          env.RAAS_USERNAME = "user"
          env.RAAS_PASSWORD = "user"
          execute("python clitest.py --suitedir testcases/suites/ --suite ${suite_to_run} --type hardware --reset --raas 193.208.80.31:${raasPort} --failure_return_value -vvv -w --log log_${raasPort}_${suiteName}")
          archive "log_${raasPort}_${suiteName}/**/*"
        }
      }
    }
  }
}