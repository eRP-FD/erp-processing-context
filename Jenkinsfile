/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
                        label 'ios' // ios nodes have ~/.netrc set up to be able to download reqmd
                    }
                    steps {
                        cleanWs()
                        commonCheckout()

                        sh """
                            #!/bin/bash -l
                            set -e
                            export LC_ALL=en_US.UTF-8
                            export LANG=en_US.UTF-8
                            doc/requirements/reqmd.sh
                            git diff --quiet -- . || (set +v; echo "ERROR: Changes done by reqmd. Please run this tool before committing. Update files:"; git status --short; exit 1)
                        """
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
                    image 'de.icr.io/erp_dev/erp-pc-ubuntu-build:2.1.3'
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
                                    def erp_build_version = sh(returnStdout: true, script: "git describe").trim()
                                    def erp_release_version = "1.10.0"
                                    sh "cd /media/erp && scripts/ci-build.sh " +
                                            "--build_version='${erp_build_version}' " +
                                            "--release_version='${erp_release_version}'"
                                }
                            }
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
                                                date
                                                glog
                                                gsl-lite
                                                gtest
                                                hiredis
                                                libcurl
                                                libpq
                                                libpqxx
                                                libxml2
                                                magic_enum
                                                openssl
                                                rapidjson
                                                redis-plus-plus
                                                zlib
                                                zstd
                                        )

                                        declare -a private_packages=(
                                                asn1c
                                                csxapi
                                                hsmclient
                                                swtpm2
                                                tpmclient
                                                tss
                                        )
                                        upload_packages() {
                                            local repo="\$1"
                                            shift
                                            local -a packages=("\$@")
                                            for package in "\${packages[@]}" ; do
                                                conan upload --all \
                                                            --remote \${repo} \
                                                            --no-overwrite all \
                                                            --confirm \
                                                            "\${package}/*"
                                            done
                                        }
                                        upload_packages erp "\${private_packages[@]}"
                                        upload_packages conan-center-binaries "\${public_packages[@]}"
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
                                dependencyTrackPublisher artifact: "jenkins-build-debug/bom.xml", projectName: "erp-processing-context",
                                    projectVersion: "${currentBuild.displayName}", synchronous: true
                                dependencyTrackPublisher artifact: "jenkins-build-debug/bom.xml", projectName: "erp-processing-context",
                                    projectVersion: "latest_${env.BRANCH_NAME}", synchronous: false
                            }
                        }
                        stage ("Run Tests (2023 profiles)") {
                            steps {
                                sh """
                                    # Run the unit and integration tests
                                    cd jenkins-build-debug/bin
                                    ./erp-test --erp_profiles="2023-07-01" --erp_instance=0 --gtest_output=xml:erp-test-2023.xml
                                """
                            }
                            post {
                                always {
                                    junit "jenkins-build-debug/bin/erp-test-2023.xml"
                                }
                            }
                        }
                        stage ("Run Tests (2022 profiles)") {
                            steps {
                                sh """
                                    cd jenkins-build-debug/bin
                                    ./erp-test --erp_profiles="2022-01-01" --erp_instance=1 --gtest_output=xml:erp-test-2022.xml
                                """
                            }
                            post {
                                always {
                                    junit "jenkins-build-debug/bin/erp-test-2022.xml"
                                }
                            }
                        }
                        stage ("Run Tests (all profiles)") {
                            steps {
                                sh """
                                    cd jenkins-build-debug/bin
                                    ./erp-test --erp_profiles="all" --erp_instance=2 --gtest_output=xml:erp-test-all.xml
                                """
                            }
                            post {
                                always {
                                    junit "jenkins-build-debug/bin/erp-test-all.xml"
                                }
                            }
                        }
                        stage ("Run Tests (fhirtools)") {
                            steps {
                                sh """
                                    cd jenkins-build-debug/bin
                                    ./fhirtools-test --gtest_output=xml:fhirtools-test.xml
                                """
                            }
                            post {
                                always {
                                    junit "jenkins-build-debug/bin/fhirtools-test.xml"
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
                                sh "scripts/ci-static-analysis.sh " +
                                        "--build-path=jenkins-build-debug " +
                                        "--source-path=. " +
                                        "--clang-tidy-bin=clang-tidy-15 " +
                                        "--output=clang-tidy.txt " +
                                        "--change-target=${env.CHANGE_TARGET} " +
                                        "--git-commit=${env.GIT_COMMIT}"
                                archiveArtifacts artifacts: 'clang-tidy.txt', allowEmptyArchive:true
                                staticAnalysis("SonarQubeeRp")
                                timeout(time: 5, unit: 'MINUTES') {
                                    waitForQualityGate abortPipeline: true
                                }
                                // Fix build folder file permissions
                                sh "chmod o+w build/"
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
                                def release_version = "1.10.0"
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
                                def release_version = "1.10.0"
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
                            }
                        }
                    }
                }
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
                        tar czf erp-tests.tar.gz jenkins-build-debug resources/test resources/integration-test scripts/test_report.xslt || [ \$? -eq 1 ]
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
