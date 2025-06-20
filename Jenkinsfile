/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

pipeline {
    agent {
        node {
            label 'dockerstage'
        }
    }

    options {
        disableConcurrentBuilds()
        skipDefaultCheckout()
        copyArtifactPermission(
            '/eRp/Integration/erp_processing_context_dev, ' +
            '/eRp/Integration/erp_processing_context_dev2, ' +
            '/eRp/Integration/erp_processing_context_box, ' +
            '/eRp/Integration/erp_processing_context_box2, ' +
            '/eRp/Integration/erp_processing_context_lu, ' +
            '/eRp/Integration/erp_processing_context_lu2'
        )
    }

    environment {
        GIT_SOURCE_CREDS = credentials('jenkins-github-erp')
    }

    stages {
        stage ('Checkout & reqmd in parallel') {
            parallel  {
                stage('Checkout') {
                    steps {
                        cleanWs()
                        commonCheckout()
                    }
                }

                stage('Verify that reqmd docs are up-to-date') {
                    agent {
                        kubernetes {
                            cloud 'toolchain-cluster'
                            yaml needTools(["OS": "ubi9/ubi:9.2-755.1697625012"])
                        }
                    }
                    steps {
                        cleanWs()
                        commonCheckout()
                        loadNexusConfiguration {
                            sh """
                                #!/bin/bash -l
                                set -e
                                export LC_ALL=en_US.UTF-8
                                export LANG=en_US.UTF-8
                                doc/requirements/reqmd.sh
                                git diff --quiet -- . || (set +v; echo "ERROR: Changes done by reqmd. Please run this tool before committing. Update files:"; git status --short; exit 1)
                            """
                        }

                    }
                    post {
                        cleanup {
                            cleanWs()
                        }
                    }
                }
            }
        }

        stage('Create Release') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                gradleCreateVersionRelease()
            }
        }

        stage('Build, test, analyze') {
            agent {
                docker {
                    label 'dockerstage'
                    image 'de.icr.io/erp_dev/erp-pc-ubuntu-build:2.2.2'
                    registryUrl 'https://de.icr.io/v2'
                    registryCredentialsId 'icr_image_puller_erp_dev_api_key'
                    reuseNode true
                    args '-u root:sudo -v jenkins-build-ccache:${HOME}/.ccache' + " -v ${env.WORKSPACE}:/media/erp"
                }
            }
            stages {
                stage ("Build") {
                    environment {
                        BUILD_WRAPPER_HOME = tool 'Sonar_Build_Wrapper_Linux_eRp'
                    }
                    steps {
                        script {
                            loadNexusConfiguration {
                                loadGithubSSHConfiguration {
                                    def erp_build_version = sh(returnStdout: true, script: "git describe").trim().toLowerCase()
                                    def erp_release_version = "1.18.0"
                                    def result = sh(returnStatus: true, script: "cd /media/erp && scripts/ci-build.sh "+
                                            "--build_version='${erp_build_version}' " +
                                            "--release_version='${erp_release_version}' " +
                                            "--with_ccache " +
                                            "--build_type=RelWithDebInfo " +
                                            "--with_hsm_mock")
                                    if (result != 0){
                                        error('build failed')
                                    }
                                    // Fix build folder file permissions
                                    sh "chmod o+w build/"
                                }
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'build/RelWithDebInfo/conan_trace.log', allowEmptyArchive:true
                        }
                    }
                }
                stage ("test, analyze in parallel"){
                    parallel  {
                        stage ("Upload Conan Packages") {
                            when {
                                anyOf {
                                    branch 'master'
                                    branch 'release/*'
                                }
                            }
                            steps {
                                script {
                                    sh """#!/bin/bash
                                        declare -a public_packages=(
                                                antlr4-cppruntime
                                                boost
                                                botan
                                                cmake
                                                date
                                                glog
                                                gsl-lite
                                                gtest
                                                hiredis
                                                libcurl
                                                libpq
                                                libpqxx
                                                libunwind
                                                libxml2
                                                magic_enum
                                                openssl
                                                rapidjson
                                                redis-plus-plus
                                                xmlsec
                                                zlib
                                                zstd
                                        )

                                        declare -a private_packages=(
                                                asn1c
                                                csxapi
                                                hsmclient
                                                tpmclient
                                                tss
                                        )
                                        upload_packages() {
                                            local repo="\$1"
                                            shift
                                            local -a packages=("\$@")
                                            for package in "\${packages[@]}" ; do
                                                conan upload --remote \${repo} --confirm "\${package}/*" \
                                                    || echo "upload "\${package}" failed"
                                            done
                                        }
                                        upload_packages erp-conan-2 "\${private_packages[@]}"
                                        #upload_packages conan-center-binaries "\${public_packages[@]}"
                                        upload_packages erp-conan-2 "\${public_packages[@]}"
                                    """
                                }
                            }
                        }
                        stage('Dependency Track') {
                            when {
                                anyOf {
                                    branch 'master'
                                    branch 'release/*'
                                }
                            }
                            steps {
                                script {
                                    String workspace = sh(returnStdout: true, script: 'pwd').trim()
                                    sh """
                                        cyclonedx convert --output-version v1_1 --input-file build/RelWithDebInfo/generators/sbom/sbom.cdx.json --output-file build/RelWithDebInfo/bom.xml
                                    """
                                    // See ERP-26151 for further details:
                                    //
                                    // dependencyTrackPublisher artifact: "build/RelWithDebInfo/bom.xml", projectName: "erp-processing-context",
                                    //     projectVersion: "${currentBuild.displayName}", synchronous: true
                                    // dependencyTrackPublisher artifact: "build/RelWithDebInfo/bom.xml", projectName: "erp-processing-context",
                                    //     projectVersion: "latest_${env.BRANCH_NAME}", synchronous: false
                                }
                            }
                        }
                        stage ("Run Tests (erp-test) Testzeitraum 2025-01-15") {
                            steps {
                                sh """
                                    # YESTERDAY_DATE=\$(date -d "yesterday" +%Y-%m-%d)
                                    # CURRENT_DATE=\$(date +%Y-%m-%d)
                                    # TOMORROW_DATE=\$(date -d "tomorrow" +%Y-%m-%d)
                                    # # Set environments for resource-views
                                    # export ERP_DARREICHUNGSFORM_1_13_VALID_UNTIL=\$YESTERDAY_DATE
                                    # export ERP_2024_10_01_VALID_FROM=\$YESTERDAY_DATE
                                    # export ERP_2024_11_01_VALID_FROM=\$YESTERDAY_DATE
                                    # export ERP_GEM_2025_01_15_VALID_FROM=\$CURRENT_DATE
                                    # export ERP_DAV_2025_01_15_VALID_FROM=\$CURRENT_DATE
                                    # env |grep ERP_
                                    # Run the unit and integration tests
                                    cd build/RelWithDebInfo/bin
                                    ./erp-test --erp_instance=3 --gtest_output=xml:erp-test-3.xml
                                """
                            }
                            post {
                                always {
                                    junit "build/RelWithDebInfo/bin/erp-test-3.xml"
                                }
                            }
                        }
                        stage ("Run Tests (fhirtools)") {
                            steps {
                                sh """
                                    find build/RelWithDebInfo
                                    cd build/RelWithDebInfo/bin
                                    ./fhirtools-test --gtest_output=xml:fhirtools-test.xml
                                """
                            }
                            post {
                                always {
                                    junit "build/RelWithDebInfo/bin/fhirtools-test.xml"
                                }
                            }
                        }
                        stage ("Run Tests (exporter)") {
                            steps {
                                sh """
                                    cd build/RelWithDebInfo/bin
                                    ./exporter-test --gtest_output=xml:exporter-test.xml
                                """
                            }
                            post {
                                always {
                                    junit "build/RelWithDebInfo/bin/exporter-test.xml"
                                }
                            }
                        }
                        stage('Static Analysis') {
                            when {
                                not {
                                    anyOf {
                                        branch 'master'
                                        branch 'release/*'
                                    }
                                }
                            }
                            steps {
                                 withSonarQubeEnv('SonarQube') {
                                    sh "cd /media/erp && scripts/ci-static-analysis.sh " +
                                            "--build-path=build/RelWithDebInfo " +
                                            "--source-path=. " +
                                            "--clang-tidy-bin=clang-tidy-19 " +
                                            "--output=clang-tidy.txt " +
                                            "--change-target=${env.CHANGE_TARGET} " +
                                            "--git-commit=${env.GIT_COMMIT}"
                                    archiveArtifacts artifacts: 'clang-tidy.txt', allowEmptyArchive:true
                                    staticAnalysis("SonarQubeeRp")
                                    timeout(time: 5, unit: 'MINUTES') {
                                        waitForQualityGate abortPipeline: true
                                    }
                                 }
                            }
                        }
                    }
                }
            }
        }

        stage('Build docker image') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                sh "cp docker/Dockerfile Dockerfile"
                script {
                    withDockerRegistry(registry: [url: 'https://de.icr.io/v2/', credentialsId: 'icr_image_pusher_erp_dev_api_key']) {
                        loadNexusConfiguration{
                            withCredentials([usernamePassword(credentialsId: "jenkins-github-erp",
                                                              usernameVariable: 'GITHUB_USERNAME',
                                                              passwordVariable: 'GITHUB_OAUTH_TOKEN')]){
                                def release_version = "1.18.0"
                                def image = docker.build(
                                    "de.icr.io/erp_dev/erp-processing-context:${currentBuild.displayName}",
                                    "--build-arg CONAN_LOGIN_USERNAME=\"${env.NEXUS_USERNAME}\" " +
                                    "--build-arg CONAN_PASSWORD=\"${env.NEXUS_PASSWORD}\" " +
                                    "--build-arg GITHUB_USERNAME=\"${env.GITHUB_USERNAME}\" " +
                                    "--build-arg GITHUB_OAUTH_TOKEN=\"${env.GITHUB_OAUTH_TOKEN}\" " +
                                    "--build-arg ERP_BUILD_VERSION=\"${currentBuild.displayName}\" " +
                                    "--build-arg ERP_RELEASE_VERSION=\"${release_version}\" " +
                                    ".")

                                sh "docker cp \$(docker create --rm de.icr.io/erp_dev/erp-processing-context:${currentBuild.displayName}):/erp/erp-processing-context.tar.gz ./erp-processing-context.tar.gz"
                                sh "docker cp \$(docker create --rm de.icr.io/erp_dev/erp-processing-context:${currentBuild.displayName}):/debug/erp/erp-processing-context-debug.tar.gz ./erp-processing-context-debug.tar.gz"
                                image.push()

                                // SBOM generation
                                sh "syft --file erp-processing-context-syft-bom.xml --output cyclonedx-xml@1.4 de.icr.io/erp_dev/erp-processing-context:${currentBuild.displayName}"
                                archiveArtifacts allowEmptyArchive: true, artifacts: '*bom.xml', fingerprint: true, followSymlinks: false, onlyIfSuccessful: true
                            }
                        }
                    }
                }
            }
        }

        stage('Build docker image (erp-medication-exporter)') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                sh "cp docker/exporter/Dockerfile Dockerfile"
                script {
                    withDockerRegistry(registry: [url: 'https://de.icr.io/v2/', credentialsId: 'icr_image_pusher_erp_dev_api_key']) {
                        loadNexusConfiguration{
                            withCredentials([usernamePassword(credentialsId: "jenkins-github-erp",
                                                              usernameVariable: 'GITHUB_USERNAME',
                                                              passwordVariable: 'GITHUB_OAUTH_TOKEN')]){
                                def release_version = "1.0.0"
                                def image = docker.build(
                                    "de.icr.io/erp_dev/erp-medication-exporter:${currentBuild.displayName}",
                                    "--build-arg CONAN_LOGIN_USERNAME=\"${env.NEXUS_USERNAME}\" " +
                                    "--build-arg CONAN_PASSWORD=\"${env.NEXUS_PASSWORD}\" " +
                                    "--build-arg GITHUB_USERNAME=\"${env.GITHUB_USERNAME}\" " +
                                    "--build-arg GITHUB_OAUTH_TOKEN=\"${env.GITHUB_OAUTH_TOKEN}\" " +
                                    "--build-arg ERP_BUILD_VERSION=\"${currentBuild.displayName}\" " +
                                    "--build-arg ERP_RELEASE_VERSION=\"${release_version}\" " +
                                    ".")

                                sh "docker cp \$(docker create --rm de.icr.io/erp_dev/erp-medication-exporter:${currentBuild.displayName}):/erp-exporter/erp-medication-exporter.tar.gz ./erp-medication-exporter.tar.gz"
                                sh "docker cp \$(docker create --rm de.icr.io/erp_dev/erp-medication-exporter:${currentBuild.displayName}):/debug/erp-exporter/erp-medication-exporter-debug.tar.gz ./erp-medication-exporter-debug.tar.gz"
                                image.push()

                                // SBOM generation
                                sh "syft --file erp-medication-exporter-syft-bom.xml --output cyclonedx-xml@1.4 de.icr.io/erp_dev/erp-medication-exporter:${currentBuild.displayName}"
                                archiveArtifacts allowEmptyArchive: true, artifacts: '*bom.xml', fingerprint: true, followSymlinks: false, onlyIfSuccessful: true
                            }
                        }
                    }
                }
            }
        }

        stage('Build blob-db-initialization docker image') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                sh "cp docker/enrolment/Dockerfile Dockerfile"
                script {
                    withDockerRegistry(registry: [url: 'https://de.icr.io/v2/', credentialsId: 'icr_image_pusher_erp_dev_api_key']) {
                        loadNexusConfiguration{
                            withCredentials([usernamePassword(credentialsId: "jenkins-github-erp",
                                                              usernameVariable: 'GITHUB_USERNAME',
                                                              passwordVariable: 'GITHUB_OAUTH_TOKEN')]){
                                def release_version = "1.18.0"
                                def image = docker.build(
                                    "de.icr.io/erp_dev/blob-db-initialization:${currentBuild.displayName}",
                                    "--build-arg CONAN_LOGIN_USERNAME=\"${env.NEXUS_USERNAME}\" " +
                                    "--build-arg CONAN_PASSWORD=\"${env.NEXUS_PASSWORD}\" " +
                                    "--build-arg GITHUB_USERNAME=\"${env.GITHUB_USERNAME}\" " +
                                    "--build-arg GITHUB_OAUTH_TOKEN=\"${env.GITHUB_OAUTH_TOKEN}\" " +
                                    "--build-arg ERP_BUILD_VERSION=\"${currentBuild.displayName}\" " +
                                    "--build-arg ERP_RELEASE_VERSION=\"${release_version}\" " +
                                    ".")

                                image.push()

                                // SBOM generation
                                sh "syft --file blob-db-initialization-syft-bom.xml --output cyclonedx-xml@1.4 de.icr.io/erp_dev/blob-db-initialization:${currentBuild.displayName}"
                                archiveArtifacts allowEmptyArchive: true, artifacts: '*bom.xml', fingerprint: true, followSymlinks: false, onlyIfSuccessful: true
                            }
                        }
                    }
                }
            }
        }

        stage('SBOM') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                sbomMerge(files: ["erp-processing-context-syft-bom.xml", "blob-db-initialization-syft-bom.xml", "build/RelWithDebInfo/bom.xml"])
            }
        }

        stage('Create Artifact For Integration Tests') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps{
                script {
                    sh """
                        git clone --depth=1 https://${GIT_SOURCE_CREDS_USR}:${GIT_SOURCE_CREDS_PSW}@github.ibmgcloud.net/eRp/erp-performancetest.git
                        cat erp-performancetest/config-templates/TestKarten-achelos/QES.pem > resources/integration-test/QES.pem
                        echo >> resources/integration-test/QES.pem
                        cat erp-performancetest/config-templates/TestKarten-achelos/QES_prv.pem >> resources/integration-test/QES.pem
                        tar czf erp-tests.tar.gz build/RelWithDebInfo resources/test resources/integration-test scripts/test_report.xslt || [ \$? -eq 1 ]
                    """
                    archiveArtifacts artifacts: 'erp-tests.tar.gz', fingerprint: true
                }
            }
        }

        stage('Publish Artifacts') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                publishArtifacts()
            }
        }

        stage('Publish Release') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                finishRelease()
            }
        }

        stage('Deployment to dev') {
            when {
                anyOf {
                    branch 'master'
                    branch 'release/*'
                }
            }
            steps {
                script {
                    if (env.BRANCH_NAME == 'master') {
                        triggerDeployment('targetEnvironment': 'dev2')
                    } else if (env.BRANCH_NAME.startsWith('release/')) {
                        triggerDeployment('targetEnvironment': 'dev')
                    }
                }
            }
        }
        stage('Integration tests against dev') {
             when {
                 anyOf {
                     branch 'master'
                     //branch 'release/*'
                 }
             }
            steps {
                build wait: false, job: '/eRp/Integration/erp_processing_context_dev2', parameters: [string(name: 'BRANCH_NAME_COPY_ARTIFACTS_BUILD_NUMBER', value: env.BUILD_NUMBER), string(name: 'ERP_BUILD_ROOT', value: '/media/erp')]
            }
        }
    }

    post {
        cleanup {
            cleanWs()
        }
        failure {
            script {
                if (env.BRANCH_NAME == 'master' || env.BRANCH_NAME.startsWith("release/")) {
                    slackSendClient(message: "Build ${env.BUILD_DISPLAY_NAME} failed for branch `${env.BRANCH_NAME}`:rotating_light: \nFor more details visit <${env.BUILD_URL}|the build page>",
                                    channel: '#erp-cpp')
                }
            }
        }
        fixed {
            script {
                if (env.BRANCH_NAME == 'master' || env.BRANCH_NAME.startsWith("release/")) {
                    slackSendClient(message: "Build is now successful again on branch `${env.BRANCH_NAME}`:green_heart:",
                                    channel: '#erp-cpp')
                }
            }
        }
    }
}
