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
        stage('Checkout') {
            steps {
                cleanWs()
                commonCheckout()
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
                    image 'de.icr.io/erp_dev/erp-pc-ubuntu-build:2.0.2'
                    registryUrl 'https://de.icr.io/v2'
                    registryCredentialsId 'icr_image_puller_erp_dev_api_key'
                    reuseNode true
                    args '-u root:sudo -v $HOME/tools:$HOME/tools' // -v $HOME/workspace/myproject:/myproject'
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
                                    def erp_release_version = "1.7.0"
                                    sh "scripts/ci-build.sh " +
                                            "--build_version='${erp_build_version}' " +
                                            "--release_version='${erp_release_version}'"
                                }
                            }
                        }
                    }
                }
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
                                        boost
                                        date
                                        glog
                                        gmp
                                        gsl-lite
                                        gtest
                                        hiredis
                                        libcurl
                                        libpq
                                        libpqxx
                                        libxml2
                                        magic_enum
                                        mpfr
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
                stage ("Run Tests") {
                    steps {
                        sh """
                            # Run the unit and integration tests
                            cd jenkins-build-debug/bin
                            ls -al
                            ./erp-test --gtest_output=xml:erp-test.xml
                        """
                    }
                    post {
                        always {
                            junit "jenkins-build-debug/bin/erp-test.xml"
                        }
                    }
                }
                stage('Static Analysis') {
                    steps {
                        sh "scripts/ci-static-analysis.sh " +
                                "--build-path=jenkins-build-debug " +
                                "--source-path=. " +
                                "--clang-tidy-bin=/usr/local/bin/clang-tidy " +
                                "--output=clang-tidy.txt"
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
                                def release_version = "1.7.0"
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
                                def release_version = "1.7.0"
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
                        tar czf erp-tests.tar.gz jenkins-build-debug resources/test resources/integration-test scripts/test_report.xslt
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
                build wait: false, job: '/eRp/Integration/erp_processing_context_dev2', parameters: [string(name: 'BRANCH_NAME_COPY_ARTIFACTS_BUILD_NUMBER', value: env.BUILD_NUMBER), string(name: 'ERP_BUILD_ROOT', value: env.WORKSPACE)]
            }
        }
    }

    post {
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
