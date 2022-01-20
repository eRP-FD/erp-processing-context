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
                gradleCreateReleaseEpa()
            }
        }

        stage('Build, test, analyze') {
            agent {
                docker {
                    label 'dockerstage'
                    image 'de.icr.io/erp_dev/erp-pc-ubuntu-build:0.0.9'
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
                                    def erp_release_version = "1.4.0"
                                    sh """
                                        # Temporary workaround for SSH host verification for GitHub. TODO: include this into the docker image build
                                        mkdir -p ~/.ssh && ssh-keyscan github.ibmgcloud.net >> ~/.ssh/known_hosts
                                        # Do not stop on address sanitizer errors.
                                        export halt_on_error=0
                                        mkdir -p jenkins-build-debug
                                        cd jenkins-build-debug
                                        pip3 install conan --upgrade
                                        conan --version
                                        conan remote clean
                                        conan remote add conan-center-binaries  https://nexus.epa-dev.net/repository/conan-center-binaries --force
                                        conan user -r conan-center-binaries -p "${env.NEXUS_PASSWORD}" "${env.NEXUS_USERNAME}"
                                        conan remote add nexus https://nexus.epa-dev.net/repository/conan-center-proxy true --force

                                        # Add nexus for IBM internal files and add credentials
                                        conan remote add erp https://nexus.epa-dev.net/repository/erp-conan-internal true --force
                                        conan user -r erp -p "${env.NEXUS_PASSWORD}" "${env.NEXUS_USERNAME}"

                                        conan profile new default --detect
                                        conan profile update settings.compiler.libcxx=libstdc++11 default
                                        # Required at least for redis++ build process.
                                        conan profile update settings.compiler.cppstd=17 default
                                        conan profile update settings.compiler.version=9 default
                                        conan profile update env.CXX=g++-9 default
                                        conan profile update env.CC=gcc-9 default
                                        conan install .. --build=missing
                                        cmake -DCMAKE_BUILD_TYPE=Debug \
                                              -DERP_BUILD_VERSION=${erp_build_version} \
                                              -DERP_RELEASE_VERSION=${erp_release_version} \
                                              -DERP_WITH_HSM_MOCK=ON \
                                              ..
                                        make clean
                                        ${BUILD_WRAPPER_HOME}/build-wrapper-linux-x86-64 --out-dir sonar-reports \
                                                cmake --build . -j 6 --target test --target production
                                    """
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
                stage ("Run Tests") {
                    steps {
                        sh """
                            # Run the unit and integration tests
                            cd jenkins-build-debug/bin
                            ./erp-test --gtest_output=xml:erp-test.xml
                        """
                    }
                    post {
                        always {
                            junit "jenkins-build-debug/bin/erp-test.xml"
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
                        dependencyTrackCpp()
                    }
                }
                stage('Static Analysis') {
                    steps {
                        sh """
                            cd jenkins-build-debug
                            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                                  -DERP_WITH_HSM_MOCK=ON \
                                  ..
                            export PYTHONIOENCODING=UTF-8
                            clang-tidy --version
                            set +e
                            /usr/local/clang_11.0.1/share/clang/run-clang-tidy.py -config "\$(< ../.clang-tidy)" -header-filter="(src|test)/.*" -p . -j9 > ../clang-tidy.txt
                            set -e
                        """
                        staticAnalysis("SonarQubeeRp")
                        timeout(time: 1, unit: 'MINUTES') {
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
                                def release_version = "1.4.0"
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
                                def release_version = "1.4.0"
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
                    } else if (env.BRANCH_NAME.startsWith('release/1.4.')) {
                        triggerDeployment('targetEnvironment': 'dev')
                    }
                }
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
