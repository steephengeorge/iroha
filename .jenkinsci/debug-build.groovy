#!/usr/bin/env groovy

def remoteFilesDiffer(f1, f2) {
  sh "curl -L -o /tmp/${env.GIT_COMMIT}/f1 --create-dirs ${f1}"
  sh "curl -L -o /tmp/${env.GIT_COMMIT}/f2 ${f2}"
  diffExitCode = sh(script: "diff -q /tmp/${env.GIT_COMMIT}/f1 /tmp/${env.GIT_COMMIT}/f2", returnStatus: true)
  if (diffExitCode == 0) {
    return false
  }
  return true
}

def doDebugBuild(coverageEnabled=false) {
  def parallelism = params.PARALLELISM
  // params are always null unless job is started
  // this is the case for the FIRST build only.
  // So just set this to same value as default. 
  // This is a known bug. See https://issues.jenkins-ci.org/browse/JENKINS-41929
  if (parallelism == null) {
    parallelism = 4
  }
  if ("arm7" in env.NODE_NAME) {
    parallelism = 1
  }
  sh "docker network create ${env.IROHA_NETWORK}"

  docker.image('postgres:9.5').run(""
    + " -e POSTGRES_USER=${env.IROHA_POSTGRES_USER}"
    + " -e POSTGRES_PASSWORD=${env.IROHA_POSTGRES_PASSWORD}"
    + " --name ${env.IROHA_POSTGRES_HOST}"
    + " --network=${env.IROHA_NETWORK}")

  def platform = sh(script: 'uname -m', returnStdout: true).trim()
  def commit = sh(script: "echo ${BRANCH_NAME} | md5sum | cut -c 1-8", returnStdout: true).trim()

  if (remoteFilesDiffer("https://raw.githubusercontent.com/hyperledger/iroha/${env.GIT_COMMIT}/docker/develop/${platform}/Dockerfile", "https://raw.githubusercontent.com/hyperledger/iroha/${env.GIT_PREVIOUS_COMMIT}/docker/develop/${platform}/Dockerfile")) {
    // develop branch Docker image has been modified
    if (BRANCH_NAME == 'develop') {
      iC = docker.build("hyperledger/iroha:${commit}", "--build-arg PARALLELISM=${parallelism} -f /tmp/${env.GIT_COMMIT}/f1 /tmp/${env.GIT_COMMIT}")
      docker.withRegistry('https://registry.hub.docker.com', 'docker-hub-credentials') {
        iC.push("${platform}-develop")
      }
    }
    // build Docker image for current branch 
    else {
      iC = docker.build("hyperledger/iroha:${commit}", "--build-arg PARALLELISM=${parallelism} -f /tmp/${env.GIT_COMMIT}/f1 /tmp/${env.GIT_COMMIT}")
    }
  }
  else {
    // reuse develop branch Docker image
    if (BRANCH_NAME == 'develop') {
      iC = docker.image("hyperledger/iroha:${platform}-develop")
      iC.pull()
    }
    else {
      // first commit in this branch or Dockerfile modified
      if (remoteFilesDiffer("https://raw.githubusercontent.com/hyperledger/iroha/${env.GIT_COMMIT}/docker/develop/${platform}/Dockerfile", "https://raw.githubusercontent.com/hyperledger/iroha/develop/docker/develop/${platform}/Dockerfile")) {
        iC = docker.build("hyperledger/iroha:${commit}", "--build-arg PARALLELISM=${parallelism} -f /tmp/${env.GIT_COMMIT}/f1 /tmp/${env.GIT_COMMIT}")
      }
      // reuse develop branch Docker image
      else {
        iC = docker.image("hyperledger/iroha:${platform}-develop")
      }
    }
  }

  iC.inside(""
    + " -e IROHA_POSTGRES_HOST=${env.IROHA_POSTGRES_HOST}"
    + " -e IROHA_POSTGRES_PORT=${env.IROHA_POSTGRES_PORT}"
    + " -e IROHA_POSTGRES_USER=${env.IROHA_POSTGRES_USER}"
    + " -e IROHA_POSTGRES_PASSWORD=${env.IROHA_POSTGRES_PASSWORD}"
    + " --network=${env.IROHA_NETWORK}"
    + " -v /var/jenkins/ccache:${CCACHE_DIR}") {

    def scmVars = checkout scm
    def cmakeOptions = ""
    if ( coverageEnabled ) {
      cmakeOptions = " -DCOVERAGE=ON "
    }
    env.IROHA_VERSION = "0x${scmVars.GIT_COMMIT}"
    env.IROHA_HOME = "/opt/iroha"
    env.IROHA_BUILD = "${env.IROHA_HOME}/build"

    sh """
      ccache --version
      ccache --show-stats
      ccache --zero-stats
      ccache --max-size=5G
    """  
    sh """
      cmake \
        -DTESTING=ON \
        -H. \
        -Bbuild \
        -DCMAKE_BUILD_TYPE=Debug \
        -DIROHA_VERSION=${env.IROHA_VERSION} \
        ${cmakeOptions}
    """
    sh "cmake --build build -- -j${parallelism}"
    sh "ccache --show-stats"
    if ( coverageEnabled ) {
      sh "cmake --build build --target coverage.init.info"
    }
    def testExitCode = sh(script: 'cmake --build build --target test', returnStatus: true)
    if (testExitCode != 0) {
      currentBuild.result = "UNSTABLE"
    }
    if ( coverageEnabled ) {
      sh "cmake --build build --target cppcheck"
      // Sonar
      if (env.CHANGE_ID != null) {
        sh """
          sonar-scanner \
            -Dsonar.github.disableInlineComments \
            -Dsonar.github.repository='hyperledger/iroha' \
            -Dsonar.analysis.mode=preview \
            -Dsonar.login=${SONAR_TOKEN} \
            -Dsonar.projectVersion=${BUILD_TAG} \
            -Dsonar.github.oauth=${SORABOT_TOKEN} \
            -Dsonar.github.pullRequest=${CHANGE_ID}
        """
      }

      sh "cmake --build build --target coverage.info"
      sh "python /tmp/lcov_cobertura.py build/reports/coverage.info -o build/reports/coverage.xml"
      cobertura autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: '**/build/reports/coverage.xml', conditionalCoverageTargets: '75, 50, 0', failUnhealthy: false, failUnstable: false, lineCoverageTargets: '75, 50, 0', maxNumberOfBuilds: 50, methodCoverageTargets: '75, 50, 0', onlyStable: false, zoomCoverageChart: false
    }

    // TODO: replace with upload to artifactory server
    // develop branch only
    if ( env.BRANCH_NAME == "develop" ) {
      //archive(includes: 'build/bin/,compile_commands.json')
    }
  }
}

return this
